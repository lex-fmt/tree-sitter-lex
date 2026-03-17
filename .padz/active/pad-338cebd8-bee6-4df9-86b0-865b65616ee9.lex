Tree Sitter Fixes

We are doing final adjustents on our tree sitter parser, to bring it to correctness, almost to parity with the reference parser.

This is used mostly for syntax highlighting in editors like vscode in nvim.
So a few mismatches are ok, in particular the ones related to the ref parser assembly stage (i.e. annotation attachments), but structural ones, namely difficulties in understnding sessions, blank lines and so forth need to be fixed.

You task: 
- Review the broken parsing tests in the tree sitter respo.
- Identify the ones related to blank line / sessions / paragraphs identification (these are related)
- Then fix the parser, be it in the scanner, grammar or both to fix it, without creating regressions.


These are very good references for you: 

comms/specs/benchmark/040-on-parsing.lex
comms/specs/trifecta/ ( all trifecta )
comms/specs/benchmark/080-gentle-introduction.lex

Dont make things up, use the lex repos parser and tests as guidance, 


Do the work, then commit and create the pr for review