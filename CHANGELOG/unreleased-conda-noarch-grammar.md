- release: the `tree-sitter` grammar now ships to the Artifact channel as a
  `noarch: generic` conda package (ARF02 / ADR-0076), resolvable through
  `[artifact-deps]` instead of the legacy cross-repo fetch. Reconciled to shipit
  v1.4.2 (conda-endpoint-gated rattler-build).
