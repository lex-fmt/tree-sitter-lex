/**
 * External scanner for tree-sitter (lex-fmt/lex).
 *
 * Handles:
 * - Indentation-based structure (INDENT/DEDENT tokens)
 * - NEWLINE emission at line boundaries and EOF
 * - Line-start detection for annotation markers (::) and list markers
 * - Session boundary detection via lookahead (_session_break)
 * - List boundary detection via lookahead (_list_start)
 * - Definition boundary detection via lookahead (_definition_subject)
 * - Emphasis delimiter validation (*strong* and _emphasis_) with flanking rules
 *
 * Block boundary lookahead:
 *   When a list marker is detected, the scanner peeks ahead to the next line.
 *   If the next line also starts with a list marker at the same indent level,
 *   the scanner emits _list_start instead of list_marker. Since paragraphs only
 *   match list_marker (not _list_start), the paragraph deterministically ends
 *   and the list starts without needing a preceding blank line.
 *
 *   Similarly, when a subject line (ending with :) is detected, the scanner
 *   peeks at the next line's indentation. If it has increased indent, the
 *   scanner emits _definition_subject instead of subject_content. Paragraphs
 *   only match subject_content, so definitions start without blank lines.
 *
 * Session break lookahead:
 *   When a blank line is encountered, the scanner peeks ahead past additional
 *   blank lines to check if the next non-blank line has increased indent.
 *   If yes, it emits _session_break (which encompasses the blank lines and
 *   indent whitespace, also pushing the new indent level onto the stack).
 *   If no, it emits a regular NEWLINE for the blank line. This eliminates
 *   the GLR ambiguity between sessions and paragraphs that caused nested
 *   sessions to parse incorrectly.
 *
 * Flanking context tracking:
 *   The scanner tracks `last_char_class` to know what preceded the current
 *   position. When the scanner returns false (grammar lexer handles the token),
 *   it infers context from `lexer->lookahead`: the grammar's _word is split into
 *   _word_alnum (always class WORD), _word_space (always class WHITESPACE), and
 *   _word_other (always class PUNCTUATION). All other grammar tokens (code_span,
 *   math_span, reference, escape_sequence, _delimiter_char) end with punctuation.
 *   So classify_char(lookahead) correctly predicts the class of the LAST character
 *   of whatever grammar token will be consumed.
 *
 * Lex uses 4-space indentation units (or 1 tab = 1 level).
 *
 * Externals order (must match grammar.js):
 *   0: _indent
 *   1: _dedent
 *   2: _newline
 *   3: annotation_marker (opening :: at line start)
 *   4: list_marker
 *   5: subject_content
 *   6: _strong_open
 *   7: _strong_close
 *   8: _emphasis_open
 *   9: _emphasis_close
 *  10: _session_break
 *  11: verbatim_content
 *  12: _list_start
 *  13: _definition_subject
 *  14: _pipe_row_start
 *  15: pipe_delimiter
 *  16: _table_separator
 *  17: annotation_close (closing :: mid-line, only inside annotation/verbatim)
 *
 * Flanking validation for emphasis delimiters:
 *   Opening: prev char must not be alphanumeric (WORD class), next must be WORD.
 *   Closing: prev must not be whitespace/none, next must not be WORD.
 *   The "prev" check uses last_char_class, which is only reliably updated when
 *   scan() returns true. To compensate, _word_alnum in grammar.js is defined as
 *   a greedy pattern that absorbs word-adjacent * and _ (e.g. word*not, snake_case)
 *   so the scanner never sees them as separate delimiter tokens.
 */

#include "tree_sitter/parser.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INDENT_DEPTH 64
/// The width of a standard Lex indent level in spaces
#define INDENT_WIDTH 4

enum TokenType {
    INDENT,
    DEDENT,
    NEWLINE,
    ANNOTATION_MARKER,
    LIST_MARKER,
    SUBJECT_CONTENT,
    STRONG_OPEN,
    STRONG_CLOSE,
    EMPHASIS_OPEN,
    EMPHASIS_CLOSE,
    SESSION_BREAK,
    VERBATIM_CONTENT,
    LIST_START,
    DEFINITION_SUBJECT,
    PIPE_ROW_START,
    PIPE_DELIMITER,
    TABLE_SEPARATOR,
    ANNOTATION_CLOSE,
    SESSION_MARKER,
};

// Character class for flanking rule context tracking.
// 0 = start-of-line/none, 1 = whitespace, 2 = punctuation, 3 = word (alnum)
#define CHAR_CLASS_NONE 0
#define CHAR_CLASS_WHITESPACE 1
#define CHAR_CLASS_PUNCTUATION 2
#define CHAR_CLASS_WORD 3

typedef struct {
    int indent_stack[MAX_INDENT_DEPTH];
    int indent_depth;
    int pending_dedents;
    bool at_line_start;
    bool emitted_eof_newline;
    uint8_t last_char_class;
    int line_indent;       // measured indent of current line (avoids re-measurement after DEDENT)
    int last_subject_indent; // captured when SUBJECT_CONTENT matches, used by verbatim_block
    bool indent_measured;  // true when line_indent is valid for the current line
    bool in_pipe_row;      // true after PIPE_ROW_START, reset at NEWLINE
} Scanner;

static uint8_t classify_char(int32_t c) {
    if (c == 0) return CHAR_CLASS_NONE;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return CHAR_CLASS_WHITESPACE;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        return CHAR_CLASS_WORD;
    return CHAR_CLASS_PUNCTUATION;
}

void *tree_sitter_lex_external_scanner_create(void) {
    Scanner *scanner = calloc(1, sizeof(Scanner));
    scanner->indent_stack[0] = 0;
    scanner->indent_depth = 0;
    scanner->pending_dedents = 0;
    scanner->at_line_start = true;
    scanner->emitted_eof_newline = false;
    scanner->last_char_class = CHAR_CLASS_NONE;
    scanner->line_indent = 0;
    scanner->last_subject_indent = 0;
    scanner->indent_measured = false;
    scanner->in_pipe_row = false;
    return scanner;
}

void tree_sitter_lex_external_scanner_destroy(void *payload) {
    free(payload);
}

unsigned tree_sitter_lex_external_scanner_serialize(void *payload,
                                                     char *buffer) {
    Scanner *scanner = (Scanner *)payload;
    unsigned offset = 0;

    buffer[offset++] = (char)scanner->indent_depth;
    buffer[offset++] = (char)scanner->pending_dedents;
    buffer[offset++] = (char)scanner->at_line_start;
    buffer[offset++] = (char)scanner->emitted_eof_newline;
    buffer[offset++] = (char)scanner->last_char_class;
    buffer[offset++] = (char)scanner->indent_measured;
    buffer[offset++] = (char)scanner->in_pipe_row;
    int16_t li = (int16_t)scanner->line_indent;
    memcpy(buffer + offset, &li, 2);
    offset += 2;
    int16_t lsi = (int16_t)scanner->last_subject_indent;
    memcpy(buffer + offset, &lsi, 2);
    offset += 2;

    for (int i = 0; i <= scanner->indent_depth; i++) {
        int16_t val = (int16_t)scanner->indent_stack[i];
        memcpy(buffer + offset, &val, 2);
        offset += 2;
    }


    return offset;
}

void tree_sitter_lex_external_scanner_deserialize(void *payload,
                                                    const char *buffer,
                                                    unsigned length) {
    Scanner *scanner = (Scanner *)payload;

    if (length == 0) {
        scanner->indent_depth = 0;
        scanner->indent_stack[0] = 0;
        scanner->pending_dedents = 0;
        scanner->at_line_start = true;
        scanner->emitted_eof_newline = false;
        scanner->last_char_class = CHAR_CLASS_NONE;
        scanner->line_indent = 0;
        scanner->last_subject_indent = 0;
        scanner->indent_measured = false;
        scanner->in_pipe_row = false;
        return;
    }

    unsigned offset = 0;
    scanner->indent_depth = (int)(unsigned char)buffer[offset++];
    scanner->pending_dedents = (int)(unsigned char)buffer[offset++];
    scanner->at_line_start = (bool)buffer[offset++];
    scanner->emitted_eof_newline = (bool)buffer[offset++];
    scanner->last_char_class = (uint8_t)(unsigned char)buffer[offset++];
    scanner->indent_measured = (bool)buffer[offset++];
    scanner->in_pipe_row = (bool)buffer[offset++];
    int16_t li;
    memcpy(&li, buffer + offset, 2);
    scanner->line_indent = li;
    offset += 2;
    int16_t lsi;
    memcpy(&lsi, buffer + offset, 2);
    scanner->last_subject_indent = lsi;
    offset += 2;



    for (int i = 0; i <= scanner->indent_depth; i++) {
        int16_t val;
        memcpy(&val, buffer + offset, 2);
        scanner->indent_stack[i] = val;
        offset += 2;
    }

}

/// Check if a character is a digit
static bool is_digit(int32_t c) { return c >= '0' && c <= '9'; }

/// Check if a character is a lowercase letter
static bool is_lower(int32_t c) { return c >= 'a' && c <= 'z'; }

/// Check if a character is an uppercase letter
static bool is_upper(int32_t c) { return c >= 'A' && c <= 'Z'; }

/// Check if a character is a roman numeral letter (upper)
static bool is_roman_upper(int32_t c) {
    return c == 'I' || c == 'V' || c == 'X' || c == 'L' || c == 'C' ||
           c == 'D' || c == 'M';
}

/// Try to match a list marker at the current position.
/// Returns true if a valid list marker was found, false otherwise.
/// On success, the lexer is positioned right after the marker+space.
static bool try_list_marker(TSLexer *lexer) {
    // Plain dash: "- "
    if (lexer->lookahead == '-') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == ' ') {
            lexer->advance(lexer, false);
            return true;
        }
        return false;
    }

    // Double-paren form: (1), (a), (IV)
    if (lexer->lookahead == '(') {
        lexer->advance(lexer, false);
        bool has_content = false;
        while (is_digit(lexer->lookahead) || is_lower(lexer->lookahead) ||
               is_roman_upper(lexer->lookahead)) {
            lexer->advance(lexer, false);
            has_content = true;
        }
        if (has_content && lexer->lookahead == ')') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == ' ') {
                lexer->advance(lexer, false);
                return true;
            }
        }
        return false;
    }

    // Numerical: 1. 1) | Alphabetical: a. a) | Roman: IV. IV)
    bool starts_digit = is_digit(lexer->lookahead);
    bool starts_lower = is_lower(lexer->lookahead);
    bool starts_upper = is_upper(lexer->lookahead);

    if (!starts_digit && !starts_lower && !starts_upper) return false;

    lexer->advance(lexer, false);

    if (starts_digit) {
        while (is_digit(lexer->lookahead)) lexer->advance(lexer, false);
        // Extended form: 1.a, 1.a.1, 2.1.3, etc.
        // Each segment after the first is dot + digits/letters/roman.
        // The trailing dot is optional: "1.a " and "1.a. " are both valid.
        bool is_extended = false;
        while (lexer->lookahead == '.') {
            lexer->advance(lexer, false);
            if (!is_digit(lexer->lookahead) && !is_lower(lexer->lookahead) &&
                !is_roman_upper(lexer->lookahead)) {
                // Period followed by space = "1. " or "1.a. " style
                if (lexer->lookahead == ' ') {
                    lexer->advance(lexer, false);
                    return true;
                }
                return false;
            }
            is_extended = true;
            while (is_digit(lexer->lookahead) ||
                   is_lower(lexer->lookahead) ||
                   is_roman_upper(lexer->lookahead)) {
                lexer->advance(lexer, false);
            }
        }
        // Extended form without trailing dot: "1.a " or "1.a.1 "
        if (is_extended && lexer->lookahead == ' ') {
            lexer->advance(lexer, false);
            return true;
        }
    } else if (starts_lower) {
        // Single lowercase letter — already consumed
    } else if (starts_upper) {
        while (is_roman_upper(lexer->lookahead)) {
            lexer->advance(lexer, false);
        }
    }

    // Expect separator: . or )
    if (lexer->lookahead == '.' || lexer->lookahead == ')') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == ' ') {
            lexer->advance(lexer, false);
            return true;
        }
    }

    return false;
}

/// Consume the rest of the line (everything up to but not including \n or EOF).
static void consume_rest_of_line(TSLexer *lexer) {
    while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
        lexer->advance(lexer, false);
    }
}

/// Scan ahead (without marking) to check if a matching delimiter exists on
/// the rest of the current line. Used to avoid emitting _strong_open or
/// _emphasis_open when no closer exists — which would produce ERROR nodes
/// for cases like "*List preceding blank" or "_not emphasized".
static bool has_matching_closer(TSLexer *lexer, int32_t delimiter) {
    while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
        if (lexer->lookahead == delimiter) {
            return true;
        }
        lexer->advance(lexer, false);
    }
    return false;
}

/// Peek ahead to check if a subsequent line starts with a list marker at the
/// same indent level. Call AFTER mark_end has been set for the current
/// list marker. Advances past content lines looking for a same-level list
/// marker. Handles nested (indented) content between list items by skipping
/// lines with indent > expected_indent. Returns true if a list marker pattern
/// is found at expected_indent. The lexer position advances past mark_end,
/// but tree-sitter resets to mark_end when the token is accepted.
static bool peek_next_line_has_list_marker(TSLexer *lexer, int expected_indent) {
    // Consume rest of current line (content after the list marker)
    while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
        lexer->advance(lexer, false);
    }
    if (lexer->eof(lexer)) return false;

    // Deep lookahead: scan through lines until we find one at expected_indent
    // or at a lesser indent (meaning we've left the list context).
    //
    // Key rule: blank lines are only skipped inside nested (indented) content.
    // A blank line at the top level (between items at expected_indent)
    // terminates the list — the lex spec says "no blank lines BETWEEN list
    // items" and "blank line terminates previous list". This prevents the
    // lookahead from crossing session boundaries (blank + indent + content
    // + blank pattern) and mistaking numbered sessions for list items.
    bool in_nested = false;
    bool pending_blank_line = false;
    for (int safety = 0; safety < 1000; safety++) {
        // Consume newline
        if (lexer->lookahead != '\n') return false;
        lexer->advance(lexer, false);

        // Measure indent of this line
        int next_indent = 0;
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
            if (lexer->lookahead == '\t') {
                next_indent += INDENT_WIDTH;
            } else {
                next_indent++;
            }
            lexer->advance(lexer, false);
        }

        // Blank line
        if (lexer->lookahead == '\n') {
            if (!in_nested) {
                // Blank line at top level might terminate the list,
                // BUT it might just precede nested content.
                // We must check the next content line's indent.
                pending_blank_line = true;
                continue;
            }
            // Inside nested content, blank lines can separate nested blocks
            continue;
        }
        if (lexer->eof(lexer)) return false;

        // Line at expected indent: check for list marker
        if (next_indent == expected_indent) {
            // If we had a blank line at the top level, and the next item
            // is at the same indent, the blank line terminated the list!
            if (pending_blank_line) return false;
            return try_list_marker(lexer);
        }

        // Line at lesser indent: we've left the list context
        if (next_indent < expected_indent) return false;

        // Line at greater indent (nested content): skip line and continue.
        // If we had a pending blank line, it's now validated as inside nested content.
        in_nested = true;
        pending_blank_line = false;
        while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
            lexer->advance(lexer, false);
        }
        if (lexer->eof(lexer)) return false;
    }

    return false;
}

/// Peek ahead to check if the next line has increased indentation or
/// starts with :: (verbatim marker form).
/// Call AFTER mark_end has been set for the current subject content.
/// The lexer should be positioned at the \n at end of the subject line.
/// Returns true if the next line's indent is greater than current_indent,
/// or if it starts with :: at the same indent (verbatim marker form).
static bool peek_next_line_has_indent(TSLexer *lexer, int current_indent) {
    // We should be at end of line (\n) or EOF
    if (lexer->eof(lexer)) return false;
    if (lexer->lookahead != '\n') return false;

    // Consume newline
    lexer->advance(lexer, false);

    // Measure indent of next line
    int next_indent = 0;
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        if (lexer->lookahead == '\t') {
            next_indent += INDENT_WIDTH;
        } else {
            next_indent++;
        }
        lexer->advance(lexer, false);
    }

    // Must not be a blank line or EOF
    if (lexer->lookahead == '\n' || lexer->eof(lexer)) return false;

    // Increased indent → definition/verbatim content follows
    if (next_indent > current_indent) return true;

    // Same indent + starts with :: → verbatim marker form (closing annotation
    // at the same level as the subject, no content between them)
    if (next_indent == current_indent && lexer->lookahead == ':') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == ':') return true;
    }

    return false;
}

/// Combined check for list-marker lines: determines if this marker starts
/// a list (2+ items) or is a session title (followed by blank + indent).
/// Returns: -1 = session pattern (suppress list token entirely),
///           0 = no list continuation found (emit LIST_MARKER),
///           1 = list continuation confirmed (emit LIST_START).
/// Call AFTER mark_end has been set for the list marker.
/// Advances past mark_end (tree-sitter resets on token accept/reject).
static int check_list_or_session(TSLexer *lexer, int expected_indent) {
    // Consume rest of current line (content after the list marker)
    while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
        lexer->advance(lexer, false);
    }
    if (lexer->eof(lexer)) return 0;

    // Consume the \n at end of current line
    lexer->advance(lexer, false);

    // Check if the immediate next line is blank AND the line after has
    // increased indent — this is the session pattern (blank + indent).
    // In lex, list items are NEVER separated by blank lines (blank
    // terminates the list). A blank after a list-marker line followed
    // by increased indent is always a session title, not a list item.
    if (lexer->lookahead == '\n') {
        // Skip blank lines
        while (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
        }
        if (lexer->eof(lexer)) return 0;  // blanks then EOF = not a session

        // Measure indent of first non-blank line
        int post_blank_indent = 0;
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
            if (lexer->lookahead == '\t')
                post_blank_indent += INDENT_WIDTH;
            else
                post_blank_indent++;
            lexer->advance(lexer, false);
        }

        // Session pattern: non-blank content at increased indent
        if (post_blank_indent > expected_indent &&
            lexer->lookahead != '\n' && !lexer->eof(lexer)) {
            return -1;
        }
        // Blank + no indent increase = not a session (e.g., dialog line
        // followed by paragraph). Return 0 (no list continuation either,
        // since blank terminated any potential list).
        return 0;
    }

    // Not blank — continue with the deep lookahead for list start.
    // This is the same logic as peek_next_line_has_list_marker but
    // starting from the already-consumed position (we're at the first
    // char of the next line).

    // Measure indent of this (next) line
    int next_indent = 0;
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        if (lexer->lookahead == '\t') {
            next_indent += INDENT_WIDTH;
        } else {
            next_indent++;
        }
        lexer->advance(lexer, false);
    }

    if (lexer->eof(lexer)) return 0;

    // Line at expected indent: check for list marker (confirms 2+ items)
    if (next_indent == expected_indent) {
        if (lexer->lookahead != '\n' && try_list_marker(lexer)) return 1;
        return 0;
    }

    // Line at lesser indent: left the list context
    if (next_indent < expected_indent) return 0;

    // Line at greater indent (nested content): deep scan through lines
    // using the same logic as peek_next_line_has_list_marker.
    bool in_nested = true;
    bool pending_blank_line = false;
    // Skip this first nested line
    while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
        lexer->advance(lexer, false);
    }
    if (lexer->eof(lexer)) return 0;

    for (int safety = 0; safety < 1000; safety++) {
        if (lexer->lookahead != '\n') return 0;
        lexer->advance(lexer, false);

        next_indent = 0;
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
            if (lexer->lookahead == '\t')
                next_indent += INDENT_WIDTH;
            else
                next_indent++;
            lexer->advance(lexer, false);
        }

        // Blank line
        if (lexer->lookahead == '\n') {
            if (!in_nested) {
                pending_blank_line = true;
                continue;
            }
            continue;
        }
        if (lexer->eof(lexer)) return 0;

        if (next_indent == expected_indent) {
            if (pending_blank_line) return 0;
            if (try_list_marker(lexer)) return 1;
            return 0;
        }
        if (next_indent < expected_indent) return 0;

        in_nested = true;
        pending_blank_line = false;
        while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
            lexer->advance(lexer, false);
        }
        if (lexer->eof(lexer)) return 0;
    }

    return 0;
}

/// Try to detect a pipe row at the current position. The lexer should be
/// positioned at a '|' character. Peeks ahead to classify the line as:
/// - separator: all content between pipes is only |, -, :, =, +, space
/// - data row: at least one more | on the line with other content
/// - not a pipe row: no second | found
/// Returns: 0 = not a pipe row, 1 = data row, 2 = separator row
/// On return, the lexer has advanced past the entire line (past mark_end).
/// Caller must set mark_end appropriately before calling.
static int classify_pipe_line(TSLexer *lexer) {
    // We're positioned after the first |. Scan rest of line.
    bool has_another_pipe = false;
    bool is_separator = true;

    while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
        int32_t c = lexer->lookahead;
        if (c == '|') has_another_pipe = true;
        if (c != '|' && c != '-' && c != ':' && c != '=' && c != '+' && c != ' ') {
            is_separator = false;
        }
        lexer->advance(lexer, false);
    }

    if (!has_another_pipe) return 0;
    if (is_separator) return 2;
    return 1;
}

bool tree_sitter_lex_external_scanner_scan(void *payload, TSLexer *lexer,
                                           const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;
    uint32_t true_start_column = lexer->get_column(lexer);

    // Emit pending DEDENT tokens
    if (scanner->pending_dedents > 0 && valid_symbols[DEDENT]) {
        scanner->pending_dedents--;
        lexer->result_symbol = DEDENT;
        return true;
    }

    // At line start, calculate indentation and detect line-start tokens
    if (scanner->at_line_start) {
        int indent;
        if (scanner->indent_measured) {
            // Indent was already measured for this line (we're re-entering
            // after a DEDENT emission). Don't re-count — the whitespace
            // was already consumed by the prior scan.
            indent = scanner->line_indent;
        } else {
            indent = 0;
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                if (lexer->lookahead == '\t') {
                    indent += INDENT_WIDTH;
                } else {
                    indent++;
                }
                lexer->advance(lexer, true); // skip
            }
            scanner->line_indent = indent;
            scanner->indent_measured = true;
       }

        // Blank line — check for session break or emit NEWLINE
        if (lexer->lookahead == '\n') {
            // Session break detection: blank line(s) followed by indent increase.
            // The scanner peeks ahead to determine if this blank line starts a
            // session boundary. If yes, emit SESSION_BREAK (encompassing the
            // blank lines + indent whitespace). If no, emit regular NEWLINE.
            if (valid_symbols[SESSION_BREAK]) {
                // Consume the first \n — this is our minimum token
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);

                // Peek ahead past additional blank lines WITHOUT updating
                // mark_end. If SESSION_BREAK fires, we'll update mark_end
                // to include everything. If not, the token only covers
                // the first blank line (so each blank becomes its own node).
                while (lexer->lookahead == '\n') {
                    lexer->advance(lexer, false);
                }

                // If EOF after blank lines, not a session break
                if (lexer->eof(lexer)) {
                    lexer->result_symbol = NEWLINE;
                    scanner->at_line_start = true;
                    scanner->indent_measured = false;
                    scanner->last_char_class = CHAR_CLASS_NONE;
                    scanner->in_pipe_row = false;
                    return true;
                }

                // Count indent of next non-blank line.
                // Advance with skip=false so characters are consumed
                // if SESSION_BREAK fires (we'll call mark_end then).
                int next_indent = 0;
                while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                    if (lexer->lookahead == '\t') {
                        next_indent += INDENT_WIDTH;
                    } else {
                        next_indent++;
                    }
                    lexer->advance(lexer, false);
                }

                // Check if next line is actually another blank line
                // (with leading whitespace)
                if (lexer->lookahead == '\n') {
                    // Not a session break — emit NEWLINE for just the
                    // first blank line (mark_end is already after it).
                    lexer->result_symbol = NEWLINE;
                    scanner->at_line_start = true;
                    scanner->indent_measured = false;
                    scanner->last_char_class = CHAR_CLASS_NONE;
                    scanner->in_pipe_row = false;
                    return true;
                }

                int current_indent =
                    scanner->indent_stack[scanner->indent_depth];
                if (next_indent > current_indent) {
                    // Fullwidth suppression: if the indent increase is
                    // sub-INDENT_WIDTH (1-3 spaces) and VERBATIM_CONTENT
                    // is valid, this is fullwidth verbatim content, not a
                    // session break. Emit NEWLINE for just the blank line
                    // so the grammar's fullwidth branch can match.
                    if (next_indent < INDENT_WIDTH &&
                        valid_symbols[VERBATIM_CONTENT]) {
                        lexer->result_symbol = NEWLINE;
                        scanner->at_line_start = true;
                        scanner->indent_measured = false;
                        scanner->last_char_class = CHAR_CLASS_NONE;
                        scanner->in_pipe_row = false;
                        return true;
                    }
                    // Session break confirmed! Update mark_end to include
                    // all blank lines + indent whitespace.
                    lexer->mark_end(lexer);
                    scanner->indent_depth++;
                    scanner->indent_stack[scanner->indent_depth] = next_indent;
                    scanner->at_line_start = false;
                    scanner->last_char_class = CHAR_CLASS_NONE;
                    lexer->result_symbol = SESSION_BREAK;
                    return true;
                }

                // Not a session break. Emit NEWLINE for just the first
                // blank line. mark_end is after the first \n only —
                // subsequent blank lines and indent whitespace are past
                // mark_end and will be re-scanned.
                lexer->result_symbol = NEWLINE;
                scanner->at_line_start = true;
                scanner->indent_measured = false;
                scanner->last_char_class = CHAR_CLASS_NONE;
                scanner->in_pipe_row = false;
                return true;
            }

            // No session break context — regular blank line NEWLINE
            if (valid_symbols[NEWLINE]) {
                lexer->advance(lexer, false);
                lexer->result_symbol = NEWLINE;
                scanner->at_line_start = true;
                scanner->indent_measured = false;
                scanner->last_char_class = CHAR_CLASS_NONE;
                scanner->in_pipe_row = false;
                return true;
            }
            return false;
        }

        // End of file — emit remaining DEDENTs, then one synthetic NEWLINE
        if (lexer->eof(lexer)) {
            if (scanner->indent_depth > 0 && valid_symbols[DEDENT]) {
                scanner->indent_depth--;
                scanner->pending_dedents = scanner->indent_depth;
                scanner->indent_depth = 0;
                lexer->result_symbol = DEDENT;
                return true;
            }
            // Emit one synthetic NEWLINE at EOF to close any pending line
            if (!scanner->emitted_eof_newline && valid_symbols[NEWLINE]) {
                scanner->emitted_eof_newline = true;
                lexer->result_symbol = NEWLINE;
                scanner->in_pipe_row = false;
                return true;
            }
            return false;
        }

        int current_indent = scanner->indent_stack[scanner->indent_depth];

        // Fullwidth verbatim content: indent 1-3 (sub-INDENT_WIDTH) is never
        // valid normal lex indentation. When VERBATIM_CONTENT is valid (the
        // grammar is exploring the fullwidth branch of verbatim_block), consume
        // all lines until :: at subject indent level as an opaque multi-line token.
        // This MUST run before indent handling to prevent spurious DEDENT/INDENT.
        if (indent > 0 && indent < INDENT_WIDTH && valid_symbols[VERBATIM_CONTENT]) {
            int subject_indent = scanner->last_subject_indent;

            // Consume the rest of the current line
            while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
                lexer->advance(lexer, false);
            }
            if (!lexer->eof(lexer)) {
                lexer->advance(lexer, false); // consume \n
            }
            lexer->mark_end(lexer);

            bool found_closing = false;

            // Consume subsequent lines until closing :: at subject indent
            while (!lexer->eof(lexer)) {
                // Measure indent of this line
                int line_indent = 0;
                while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                    if (lexer->lookahead == '\t') {
                        line_indent += INDENT_WIDTH;
                    } else {
                        line_indent++;
                    }
                    // Consume indent as part of the VERBATIM_CONTENT token.
                    lexer->advance(lexer, false);
                }

                // Blank line — include in content
                if (lexer->lookahead == '\n') {
                    lexer->advance(lexer, false);
                    lexer->mark_end(lexer);
                    continue;
                }

                if (lexer->eof(lexer)) break;

                // Check for closing :: at subject indent
                if (line_indent == subject_indent && lexer->lookahead == ':') {
                    lexer->advance(lexer, false);
                    if (lexer->lookahead == ':') {
                        // Found closing annotation — stop opaque content.
                        found_closing = true;
                        break;
                    }
                    // False positive (single colon) — still content
                    consume_rest_of_line(lexer);
                    if (!lexer->eof(lexer)) {
                        lexer->advance(lexer, false); // \n
                    }
                    lexer->mark_end(lexer);
                    continue;
                }

                // Regular content line
                consume_rest_of_line(lexer);
                if (!lexer->eof(lexer)) {
                    lexer->advance(lexer, false); // \n
                }
                lexer->mark_end(lexer);
            }

            if (!found_closing) {
                // We reached EOF without finding the :: annotation.
                // This means we are likely inside a rogue GLR fork with an obsolete subject_indent!
                // Failing here culls the rogue fork cleanly without poisoning the rest of the file.
                return false;
            }

            scanner->at_line_start = true;
            scanner->indent_measured = false;
            scanner->last_char_class = CHAR_CLASS_NONE;
            lexer->result_symbol = VERBATIM_CONTENT;
            return true;
        }

        // Handle indentation changes
        if (indent > current_indent) {
            // Verbatim closing annotations can be deeply indented relative to
            // the grammar's current context if the outer list ended prematurely.
            // Prioritize them over INDENT to close the verbatim block!
            if (valid_symbols[ANNOTATION_MARKER] && lexer->lookahead == ':') {
                // Let it fall through to the annotation marker detection
            } else if (valid_symbols[INDENT]) {
                scanner->indent_depth++;
                scanner->indent_stack[scanner->indent_depth] = indent;
                lexer->result_symbol = INDENT;
                scanner->at_line_start = false;
                scanner->last_char_class = CHAR_CLASS_NONE;
                return true;
            }
            // If INDENT is not valid, fall through to content detection
        } else if (indent < current_indent) {
            if (valid_symbols[DEDENT]) {
                int dedents = 0;
                while (scanner->indent_depth > 0 &&
                       scanner->indent_stack[scanner->indent_depth] > indent) {
                    scanner->indent_depth--;
                    dedents++;
                }
                if (dedents > 1) {
                    scanner->pending_dedents = dedents - 1;
                }
                lexer->result_symbol = DEDENT;
                // Keep at_line_start=true so next scan can detect
                // line-start tokens (annotation_marker, etc.)
                return true;
            }
        }

        scanner->at_line_start = false;
        scanner->last_char_class = CHAR_CLASS_NONE;

        // After handling indentation, try to detect line-start tokens.
        lexer->mark_end(lexer);
        true_start_column = lexer->get_column(lexer);

        // Save the first content char for context tracking. If we return
        // false later, the grammar will consume from here, and this char's
        // class tells us what the grammar's token class will be.
        int32_t line_start_char = lexer->lookahead;

        // Try annotation marker: :: at line start.
        // Only emit when the line has a closing :: (valid annotation/verbatim).
        // Lines like ":: marker. See specs/..." without closing :: are prose
        // text that happens to start with :: — not annotations.
        if (valid_symbols[ANNOTATION_MARKER] && lexer->lookahead == ':') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == ':') {
                lexer->advance(lexer, false);
                // mark_end after "::" — this is our token if we return true
                lexer->mark_end(lexer);
                if (lexer->lookahead == ' ') {
                    lexer->advance(lexer, false);
                    lexer->mark_end(lexer); // extend token to ":: "
                }
                // Peek rest of line for closing :: (advance without mark_end)
                int32_t prev = 0;
                bool has_closing = false;
                while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
                    if (prev == ':' && lexer->lookahead == ':') {
                        has_closing = true;
                        break;
                    }
                    prev = lexer->lookahead;
                    lexer->advance(lexer, false);
                }
                if (has_closing) {
                    // Valid annotation line — token ends at mark_end (":: ")
                    lexer->result_symbol = ANNOTATION_MARKER;
                    return true;
                }
                // No closing :: — not an annotation. Return false.
                // Position resets to before "::" (scan start), so the
                // grammar lexer handles ":" as regular text.
                scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                return false;
            }
            // Single colon — not an annotation
            return false;
        }

        // Try pipe row: | at line start with at least one more | on the line.
        // TABLE_SEPARATOR covers the full line; PIPE_ROW_START covers just the
        // first |. PIPE_ROW_START also sets in_pipe_row so subsequent | characters
        // are emitted as PIPE_DELIMITER tokens.
        if ((valid_symbols[PIPE_ROW_START] || valid_symbols[TABLE_SEPARATOR]) &&
            lexer->lookahead == '|') {
            lexer->advance(lexer, false);  // consume |
            lexer->mark_end(lexer);        // mark after first | (for PIPE_ROW_START)

            int kind = classify_pipe_line(lexer);

            if (kind == 2 && valid_symbols[TABLE_SEPARATOR]) {
                lexer->mark_end(lexer);  // extend to EOL for full separator
                lexer->result_symbol = TABLE_SEPARATOR;
                scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                return true;
            }
            if (kind >= 1 && valid_symbols[PIPE_ROW_START]) {
                // mark_end is after first |
                scanner->in_pipe_row = true;
                lexer->result_symbol = PIPE_ROW_START;
                scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                return true;
            }

            // Not a pipe row — return false, position resets
            scanner->last_char_class = classify_char('|');
            return false;
        }

        // When the first char is an emphasis delimiter (* or _), we need to
        // decide between emphasis and full-line tokens (subject_content).
        // Strategy: scan the line once to check for trailing :. Use mark_end
        // to control the emitted token's span — either 1 char (emphasis) or
        // the whole line (subject_content).
        if ((lexer->lookahead == '*' || lexer->lookahead == '_') &&
            (valid_symbols[STRONG_OPEN] || valid_symbols[EMPHASIS_OPEN])) {
            int32_t delimiter = lexer->lookahead;
            lexer->advance(lexer, false);        // skip delimiter (now at X+1)
            int32_t next_char = lexer->lookahead; // char after delimiter
            lexer->mark_end(lexer);               // mark at X+1 (1-char token)

            // Scan rest of line to check for trailing : and matching closer
            int32_t last_char = next_char;
            int32_t last_nonws = (next_char != ' ' && next_char != '\t') ? next_char : 0;
            bool has_closer = false;
            while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
                if (lexer->lookahead == delimiter) has_closer = true;
                last_char = lexer->lookahead;
                if (lexer->lookahead != ' ' && lexer->lookahead != '\t') {
                    last_nonws = lexer->lookahead;
                }
                lexer->advance(lexer, false);
            }

            if (last_nonws == ':') {
                // Subject line — check for definition subject first
                lexer->mark_end(lexer);  // mark at EOL for full line
                if (valid_symbols[DEFINITION_SUBJECT]) {
                    if (peek_next_line_has_indent(lexer, scanner->line_indent)) {
                        scanner->last_subject_indent = scanner->line_indent;
                        lexer->result_symbol = DEFINITION_SUBJECT;
                        return true;
                    }
                }
                if (valid_symbols[SUBJECT_CONTENT]) {
                    scanner->last_subject_indent = scanner->line_indent;
                    lexer->result_symbol = SUBJECT_CONTENT;
                    return true;
                }
            }

            // Not a subject. Try emphasis open — mark_end is at X+1.
            // Only emit if a matching closer exists on this line,
            // otherwise the unclosed delimiter produces ERROR nodes.
            if (has_closer && delimiter == '*' && valid_symbols[STRONG_OPEN] &&
                classify_char(next_char) == CHAR_CLASS_WORD) {
                scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                lexer->result_symbol = STRONG_OPEN;
                return true;
            }
            if (has_closer && delimiter == '_' && valid_symbols[EMPHASIS_OPEN] &&
                classify_char(next_char) == CHAR_CLASS_WORD) {
                scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                lexer->result_symbol = EMPHASIS_OPEN;
                return true;
            }

            // Neither subject nor valid emphasis. mark_end is at X+1.
            // Return false — position resets to after the delimiter.
            scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
            return false;
        }

        // Try list marker: just the marker portion (- , 1. , a) , etc.)
        // Content after the marker is handled by the grammar's text_content
        // rule, which decomposes inline elements (bold, references, etc.).
        if (valid_symbols[LIST_MARKER] || valid_symbols[LIST_START]) {
            if (try_list_marker(lexer)) {
                lexer->mark_end(lexer);

                // Combined check: session pattern (-1), list start (1), or neither (0).
                int result = check_list_or_session(lexer, scanner->line_indent);

                if (result == -1 && valid_symbols[SESSION_MARKER]) {
                    // Session pattern (blank after line) — emit SESSION_MARKER
                    // which only _session_title accepts, eliminating GLR
                    // competition from dialog_line and _list_start_item.
                    lexer->result_symbol = SESSION_MARKER;
                    scanner->last_char_class = CHAR_CLASS_WHITESPACE;
                    return true;
                }

                if (result == 1 && valid_symbols[LIST_START]) {
                    lexer->result_symbol = LIST_START;
                    scanner->last_char_class = CHAR_CLASS_WHITESPACE;
                    return true;
                }

                if (valid_symbols[LIST_MARKER]) {
                    lexer->result_symbol = LIST_MARKER;
                    scanner->last_char_class = CHAR_CLASS_WHITESPACE;
                    return true;
                }
            }
            // Not a list marker — fall through to subject_content check.
            // try_list_marker may have advanced the position, but the
            // subject_content scan continues from there to EOL.
        }

        // Try subject content: entire line ending with : (ignoring trailing whitespace)
        // Must NOT start with :: (annotation marker) to avoid shadowing block annotations.
        if (valid_symbols[SUBJECT_CONTENT] || valid_symbols[DEFINITION_SUBJECT]) {
            bool starts_with_annotation = false;
            // The lexer position here is after potentially scanning a list marker (try_list_marker),
            // but if we are checking for a bare subject line, we should verify the start of the line.
            // However, list markers explicitly block subject content if they match.
            // If try_list_marker returned false, we are back at mark_end (which is line_start).
            // Let's peek for ::
            if (lexer->lookahead == ':') {
                lexer->advance(lexer, false);
                if (lexer->lookahead == ':') {
                    starts_with_annotation = true;
                }
            }
            // Reset position to scan to end of line
            lexer->mark_end(lexer);

            if (!starts_with_annotation) {
                int32_t last_nonws = 0;
                while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
                    if (lexer->lookahead != ' ' && lexer->lookahead != '\t') {
                        last_nonws = lexer->lookahead;
                    }
                    lexer->advance(lexer, false);
                }
                if (last_nonws == ':') {
                    lexer->mark_end(lexer);

                    // If DEFINITION_SUBJECT is valid, peek ahead for indent
                    if (valid_symbols[DEFINITION_SUBJECT]) {
                        if (peek_next_line_has_indent(lexer, scanner->line_indent)) {
                            lexer->result_symbol = DEFINITION_SUBJECT;
                            return true;
                        }
                    }

                    if (valid_symbols[SUBJECT_CONTENT]) {
                        lexer->result_symbol = SUBJECT_CONTENT;
                        return true;
                    }
                }
            }
            // Line doesn't end with : or starts with :: — return false, position resets to
            // mark_end (line start). Set last_char_class for the grammar
            // token that will be consumed at that position.
            scanner->last_char_class = classify_char(line_start_char);
            return false;
        }

        scanner->last_char_class = classify_char(line_start_char);
        return false;
    }

    // === Not at line start ===

    // Pipe delimiter: | inside an active pipe row. Checked first because
    // pipe delimiters are the most common mid-line token in table content.
    if (scanner->in_pipe_row && valid_symbols[PIPE_DELIMITER] &&
        lexer->lookahead == '|') {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        lexer->result_symbol = PIPE_DELIMITER;
        scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
        return true;
    }

    // Pipe row start: | at content start (e.g., first line after INDENT).
    // Same detection as at-line-start but in the not-at-line-start branch
    // because INDENT sets at_line_start=false before returning.
    if ((valid_symbols[PIPE_ROW_START] || valid_symbols[TABLE_SEPARATOR]) &&
        lexer->lookahead == '|') {
        lexer->mark_end(lexer);
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);  // mark after first |

        int kind = classify_pipe_line(lexer);

        if (kind == 2 && valid_symbols[TABLE_SEPARATOR]) {
            lexer->mark_end(lexer);
            lexer->result_symbol = TABLE_SEPARATOR;
            scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
            return true;
        }
        if (kind >= 1 && valid_symbols[PIPE_ROW_START]) {
            scanner->in_pipe_row = true;
            lexer->result_symbol = PIPE_ROW_START;
            scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
            return true;
        }

        // Not a pipe row — fall through (position resets)
        return false;
    }

    // Try list marker (e.g., first line after INDENT in a definition body)
    if (valid_symbols[LIST_MARKER] || valid_symbols[LIST_START]) {
        lexer->mark_end(lexer);
        if (try_list_marker(lexer)) {
            lexer->mark_end(lexer);

            // Use the current indent from the stack, not scanner->line_indent
            // which may be stale after _session_break (it records the blank
            // line's indent, not the content line's indent).
            int current_indent = scanner->indent_stack[scanner->indent_depth];
            int result = check_list_or_session(lexer, current_indent);

            if (result == -1 && valid_symbols[SESSION_MARKER]) {
                lexer->result_symbol = SESSION_MARKER;
                scanner->last_char_class = CHAR_CLASS_WHITESPACE;
                return true;
            }

            if (result == 1 && valid_symbols[LIST_START]) {
                lexer->result_symbol = LIST_START;
                scanner->last_char_class = CHAR_CLASS_WHITESPACE;
                return true;
            }

            if (valid_symbols[LIST_MARKER]) {
                lexer->result_symbol = LIST_MARKER;
                scanner->last_char_class = CHAR_CLASS_WHITESPACE;
                return true;
            }
        }
        // Not a list marker — fall through
    }

    // Try subject content: entire line ending with : (ignoring trailing whitespace)
    // Must NOT start with :: (annotation marker) to avoid shadowing block annotations.
    if (valid_symbols[SUBJECT_CONTENT] || valid_symbols[DEFINITION_SUBJECT]) {
        lexer->mark_end(lexer);
        
        bool starts_with_annotation = false;
        if (lexer->lookahead == ':') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == ':') {
                starts_with_annotation = true;
            }
        }
        
        // Reset position to scan to end of line
        lexer->mark_end(lexer);
        
        if (!starts_with_annotation) {
            int32_t last_nonws = 0;
            while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
                if (lexer->lookahead != ' ' && lexer->lookahead != '\t') {
                    last_nonws = lexer->lookahead;
                }
                lexer->advance(lexer, false);
            }
            if (last_nonws == ':') {
                lexer->mark_end(lexer);
                // Remove the debug print as we don't need it

                // Use indent stack (not stale line_indent) for the same
                // reason as the list marker check — after _session_break,
                // line_indent holds the blank line's indent (0) not the
                // content line's indent.
                int subject_indent = scanner->indent_stack[scanner->indent_depth];
                if (valid_symbols[DEFINITION_SUBJECT]) {
                    if (peek_next_line_has_indent(lexer, subject_indent)) {
                        scanner->last_subject_indent = subject_indent;
                        lexer->result_symbol = DEFINITION_SUBJECT;
                        return true;
                    }
                }

                if (valid_symbols[SUBJECT_CONTENT]) {
                    scanner->last_subject_indent = subject_indent;
                    lexer->result_symbol = SUBJECT_CONTENT;
                    return true;
                }
            }
        }
        // Line doesn't end with : or starts with :: — return false, position resets
        return false;
    }

    // Check for closing :: (annotation_close). Only valid inside annotation
    // or verbatim grammar rules — never at block level, so :: in prose text
    // is never intercepted here.
    if (valid_symbols[ANNOTATION_CLOSE] && lexer->lookahead == ':') {
        lexer->mark_end(lexer);
        lexer->advance(lexer, false);
        if (lexer->lookahead == ':') {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            // Skip trailing space after ::
            if (lexer->lookahead == ' ') {
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
            }
            lexer->result_symbol = ANNOTATION_CLOSE;
            return true;
        }
        // Single colon — not a closing marker, don't consume
        return false;
    }

    // ===== Emphasis delimiter detection =====

    // Opening * for strong
    if (valid_symbols[STRONG_OPEN] && lexer->lookahead == '*') {
        // Flanking: prev must not be word char, next must be word char
        if (scanner->last_char_class != CHAR_CLASS_WORD) {
            lexer->mark_end(lexer);
            lexer->advance(lexer, false);
            if (classify_char(lexer->lookahead) == CHAR_CLASS_WORD) {
                // Check that a matching * exists on the rest of the line.
                // Without this, unclosed * (e.g., "*List..." or "mass*accel")
                // would emit _strong_open and produce ERROR nodes.
                lexer->mark_end(lexer);
                if (has_matching_closer(lexer, '*')) {
                    lexer->result_symbol = STRONG_OPEN;
                    scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                    return true;
                }
                // No closer found — don't emit, let grammar handle * as text
            }
        }
    }

    // Closing * for strong
    if (valid_symbols[STRONG_CLOSE] && lexer->lookahead == '*') {
        // Flanking: prev must not be whitespace/none, next must not be word char
        if (scanner->last_char_class != CHAR_CLASS_NONE &&
            scanner->last_char_class != CHAR_CLASS_WHITESPACE) {
            lexer->mark_end(lexer);
            lexer->advance(lexer, false);
            if (classify_char(lexer->lookahead) != CHAR_CLASS_WORD) {
                lexer->mark_end(lexer);
                lexer->result_symbol = STRONG_CLOSE;
                scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                return true;
            }
        }
    }

    // Opening _ for emphasis
    if (valid_symbols[EMPHASIS_OPEN] && lexer->lookahead == '_') {
        if (scanner->last_char_class != CHAR_CLASS_WORD) {
            lexer->mark_end(lexer);
            lexer->advance(lexer, false);
            if (classify_char(lexer->lookahead) == CHAR_CLASS_WORD) {
                // Check that a matching _ exists on the rest of the line
                lexer->mark_end(lexer);
                if (has_matching_closer(lexer, '_')) {
                    lexer->result_symbol = EMPHASIS_OPEN;
                    scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                    return true;
                }
                // No closer found — don't emit, let grammar handle _ as text
            }
        }
    }

    // Closing _ for emphasis
    if (valid_symbols[EMPHASIS_CLOSE] && lexer->lookahead == '_') {
        if (scanner->last_char_class != CHAR_CLASS_NONE &&
            scanner->last_char_class != CHAR_CLASS_WHITESPACE) {
            lexer->mark_end(lexer);
            lexer->advance(lexer, false);
            if (classify_char(lexer->lookahead) != CHAR_CLASS_WORD) {
                lexer->mark_end(lexer);
                lexer->result_symbol = EMPHASIS_CLOSE;
                scanner->last_char_class = CHAR_CLASS_PUNCTUATION;
                return true;
            }
        }
    }

    // NEWLINE or EOF
    if (valid_symbols[NEWLINE]) {
        if (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
            lexer->result_symbol = NEWLINE;
            scanner->at_line_start = true;
            scanner->indent_measured = false;
            scanner->last_char_class = CHAR_CLASS_NONE;
            scanner->in_pipe_row = false;
            return true;
        }
        if (lexer->eof(lexer) && !scanner->emitted_eof_newline) {
            scanner->emitted_eof_newline = true;
            lexer->result_symbol = NEWLINE;
            scanner->at_line_start = true;
            scanner->indent_measured = false;
            scanner->in_pipe_row = false;
            scanner->last_char_class = CHAR_CLASS_NONE;
            return true;
        }
    }

    // No external token matched. The grammar lexer will consume the next token.
    // Infer last_char_class from lookahead — because the grammar's _word is split
    // into _word_alnum (ends with alnum → WORD), _word_space (ends with space →
    // WHITESPACE), and _word_other (ends with punct → PUNCTUATION), and all other
    // grammar inline tokens end with punctuation, classify_char(lookahead) predicts
    // the last-character class of whatever token the grammar will consume.
    scanner->last_char_class = classify_char(lexer->lookahead);

    return false;
}
