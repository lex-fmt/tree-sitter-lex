---
status: accepted
---

# Injected-language resolution belongs to the host editor

tree-sitter-lex ships the Lex grammar and its queries — nothing else. Each editor
integration resolves `:: lang ::` injections through its own native mechanism, so this
repo stops publishing `shared/embedded-grammars.json`, the curated list of third-party
grammars editors should bundle. VS Code is the one exception, and there the parser set is
that repo's private build detail, not a cross-repo contract.

## Why

The manifest was never a technical requirement. It was introduced by commit `804bf7a`
(2026-04-26, "Add embedded-grammars manifest as the cross-editor source of truth") with no
ADR and no design doc, and its recorded rationale is purely organizational de-duplication:
"Until now that list lived only in vscode's `download-embedded-grammars.sh` as a hardcoded
bash array; lexed didn't have one yet, and the two were on track to drift the moment a
grammar was added or version-bumped." Nothing in it claims editors cannot resolve
injections themselves.

Nothing in the grammar constrains the injectable set. `queries/injections.scm` lifts
`@injection.language` from the `annotation_header` text; the only filter is
`#not-match? "^\s*table"`. The manifest's own `description` field concedes the point: "Lex
isn't a gatekeeper of formats — this is a packaging convenience, not a contract about which
languages 'belong'. Editors are free to bundle more, fewer, or different grammars;
injection still works for any language the host can resolve."

Two of three consumers already behave this way. **nvim** never extracts the manifest and
registers exactly one language (`lua/lex/treesitter.lua:192`); injections resolve against
the user's `:TSInstall` parsers on runtimepath, and a missing parser is silent by design
(Neovim's `languagetree.lua:1015-1018` gates on a `parser/<lang>.*` rtp glob). **Zed** does
not consume the release artifact at all — it clones and builds the grammar from
`[grammars.lex]` in `extension.toml`; `app-bin/gen-injections.py` enumerates 13 language
names only because Zed lacks `#gsub!` support, and ships none of them.

This is also what the ecosystem does. tree-sitter-markdown's entire fenced-code story is a
four-line injections query with no grammar dependencies. Microsoft's own `markdown-basics`
has 62 fenced-block rules, 208 language aliases and 48 `embeddedLanguages` entries and
ships **zero** grammars — it emits `{"include": "source.python"}` and lets the registry
resolve it. tree-sitter-asciidoc names ~30 diagram languages in an `#any-of?` predicate and
ships none. No markup grammar in the ecosystem vendors parsers for injected languages, and
there is no editor-agnostic mechanism to declare such a dependency: `tree-sitter.json`'s
`injection-regex` runs the opposite direction (the injected grammar advertises the names it
answers to) and the schema has no `requires` field. Per-host indexes — nvim-treesitter's
`parsers.lua`, Helix's `[[grammar]]`, Zed's `[grammars.X]` — are where this is solved.

## The VS Code exception

lex-fmt/vscode must ship parsers, and the reason is durable and non-obvious enough to
record. VS Code exposes no API that reaches the host's TextMate grammars or another
extension's tokenizer. LSP semantic tokens were measured and rejected (commit `8fd2360`:
Pylance returned nine tokens for a python verbatim block — "six variable, two parameter,
one function — and zero of the keyword/string/comment/number types we actually want").

The TextMate embedded-scope path is blocked by a collision between Lex's syntax and the
engine's execution model: **Lex names the language in the closing annotation** — `:: python
::` comes *after* the code — while a TextMate `begin` rule must commit to
`include: source.python` before it scans the content, and vscode-textmate is forward-only
and line-at-a-time. `Subject:` openers compound this, being ambiguous with definitions and
tables on the single line `begin` can see. This is reasoning about the engine's documented
execution model rather than a statement from VS Code's documentation, but it is corroborated
in-repo: vscode already failed at the simpler version of the same problem, and scoping
verbatim bodies for CSpell remains an open known limitation (`CHANGELOG/0.10.9.md:38-45`).

Note the scope of that limitation. It is a TextMate-engine constraint, not a flaw in Lex's
grammar design — tree-sitter injection is order-independent (Zed's `syntax_map.rs` takes
min/max over the content and language node ranges precisely so the language node may follow
the content; Neovim assigns by capture name with no positional constraint).

Consequently vscode's parser set is that repo's private build detail, sourced as a normal
npm dependency — prebuilt tree-sitter wasm is published to npm (`@repomix/tree-sitter-wasms`,
`sourcegraph/tree-sitter-wasms`) — not a conda package and not a contract with this repo.
That is why `deps.json` / `fetch-deps` can die there.

## The lexed trade-off

lexed reverts to its Monaco Monarch code path. It already imports `monaco-editor` 0.55.1 as
the full ESM bundle — all 82 basic-language contributions, no allowlist, side-effect imports
Vite cannot tree-shake — so Monaco tokenizers for the five manifest languages are already in
the shipped bytes, and the Monarch path is complete and tested at
`packages/monaco-inline-injections/src/monaco/injection_highlighter.ts:104-190`. It was the
original behaviour until commit `95581f0` replaced it "to bring lexed to parity with
vscode", with the note that "languages outside the bundle get no highlighting — the agreed
contract". Net effect: coverage regressed from 82 languages to 5.

That file also carries the objection a future reader will find first
(`injection_highlighter.ts:48-49`): "The Monaco fallback is fine for prototyping but
inaccurate compared with tree-sitter." It is true, and we accept it. The owner has
explicitly accepted tokenizer-fidelity-only for lexed: Monarch fidelity on five languages is
the price of 82-language coverage at zero maintenance cost. Do not reverse this on the
strength of that comment alone.

## Considered Options

- **Status quo — keep the curated five-grammar manifest here.** Rejected: it encodes an
  allowlist the grammar does not have, and its stated rationale (de-duplicating one bash
  array) evaporates once vscode is the only host that needs a list.
- **Issue #104's plan — package the five as noarch conda packages on our own channel.**
  Rejected: it is the status quo with more machinery. It buys a delivery mechanism for a
  list that should not exist, and makes this repo the maintainer of five foreign grammars'
  release cadence.
- **Vendor the wasm into each consumer.** Rejected for nvim/Zed/lexed — all three already
  have a native resolution path, so vendoring adds ~3.3 MB and a bump treadmill to replace
  something that already works. It is what vscode ends up doing, but as its own dependency.
- **For vscode specifically, vendor `vscode-textmate` + `vscode-oniguruma` and run TextMate
  grammars over verbatim interiors.** Technically real — the repo already does exactly this
  in `test/unit/spellcheck-scopes.test.ts` — but it trades 3.3 MB of parsers for vendored
  `.tmLanguage.json` files plus the oniguruma wasm. It does not avoid shipping per-language
  data, so it does not change the conclusion.

## Consequences

- **Ordering constraint (hard).** `shared/embedded-grammars.json` is `required = true` in
  the release payload (`.shipit.toml:194`) and build-breaking for lexed
  (`app-bin/check-deps.mjs:122`). It must be deleted from this repo **last**, only after
  lexed reverts to Monaco and vscode relocates its list into its own dependencies. Deleting
  it first breaks both a release and a consumer build.
- **What gets deleted here, in that final step:** `shared/embedded-grammars.json` and its
  payload entry, `app-bin/bump-grammars.sh`, `app-bin/smoke-grammars.sh`,
  `.github/workflows/quarterly-grammar-bump.yml`, and the smoke hook on the test lane
  (`pixi.toml:31`).
- **`queries/injections.scm` is unchanged.** It is already correct and open-ended; this
  decision removes packaging, not behaviour.
- **Coverage changes per host:** lexed 5 → 82 languages (lower fidelity); nvim and Zed
  unaffected; vscode unchanged in behaviour, changed in where its list lives.
- **This repo stops tracking upstream grammar releases.** No quarterly bump, no smoke check
  against `tree-sitter/tree-sitter-<lang>` GitHub assets, and no obligation to answer "why
  isn't language X supported?" — the answer becomes "ask your editor".
- **Scope of lex-fmt/tree-sitter-lex#104 collapses.** This decision supersedes its plan; the
  issue is not closed here, and the downstream editor changes it implies are separate work.
- **Execution is tracked under epic INJ001**, whose workstreams carry out the ordering
  constraint above across this repo, lexed and vscode.
