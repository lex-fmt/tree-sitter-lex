- ci: remove the legacy release path — the release.yml caller
  (tree-sitter.yml@v3, incl. its notify-downstreams wiring), the
  on-upstream-released legacy cascade handler, the retired
  copilot-review.yml caller, and bin/install-release-core.
  shipit-release.yml is now the only release surface (replacement
  proof: rc 0.12.0-release-rc, run 29231015838).
