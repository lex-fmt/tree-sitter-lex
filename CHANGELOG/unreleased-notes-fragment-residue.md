- chore: the root `UNRELEASED.md` deleted — release notes live in
  `CHANGELOG/unreleased-<slug>.md` fragments, which is what shipit's release
  `prepare` coalesces (zero fragments is a refused, empty release). That file
  was residue of the retired `arthur-debert/release` tooling: its own header
  named `scripts/create-release` as its consumer, and no such script has existed
  here since the shipit cutover. Nothing read it, but it read as authoritative,
  which is how every note above ended up in it instead of in a fragment, leaving
  this cut with nothing to coalesce.
- chore: the `.gitignore` `/bin/*` block dropped — the last of the same
  residue. It ignored all of `bin/` and then un-ignored six thin callers
  (`check`, `build`, `changelog`, `changelog-add`, `changelog-cut`,
  `changelog-render`) that the retired tooling supplied and that have not
  existed here for releases; what `bin/` actually holds now (`shipit`,
  `setup-dev-env.sh`, `pr-loop-guard`) was tracked in spite of it. Live cost,
  not just clutter: any NEW file under `bin/` — including one a `shipit install`
  reconcile writes — was silently ignored.
