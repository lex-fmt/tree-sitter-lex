- ci: adopt the shipit release pipeline (shipit-release.yml caller +
  declared tree-sitter artifact) — releases still ship the same
  `tree-sitter.tar.gz` asset and notify the editor repos
  (vscode/nvim/lexed) on real releases; the legacy release.yml caller
  remains until the release-candidate proof
