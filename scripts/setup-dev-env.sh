#!/usr/bin/env bash
# scripts/setup-dev-env.sh — per-session dev-environment setup, invoked by
# the SessionStart hook in .claude/settings.json.
#
# Cloud-only: local sessions exit early (devs already have their env set up).
# Detects stack by filesystem signals — works for rust, node-flavored
# (npm/yarn/pnpm), ruby (bundle), and nvim/zed/static-site (no project
# deps, just lefthook wiring). Stack-specific extras (e.g. resource
# download scripts, submodule init) can be added below the universal
# section as needed for the particular repo.
#
# Idempotent — safe to re-run. Errors are best-effort: a failure in one
# step doesn't abort the rest (e.g. transient registry hiccup on cargo
# fetch shouldn't block the lefthook install).

set -euo pipefail

# Cloud-only gate. Local sessions already have their env set up.
[ "${CLAUDE_CODE_REMOTE:-}" = "true" ] || exit 0

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "${REPO_ROOT}"

# 0. Git submodules. Fresh clones in cloud sessions don't init submodules
# by default, but tests / generators often need their content (e.g. spec
# fixtures). Idempotent — no-op when there are no submodules.
if [ -f .gitmodules ]; then
  git submodule update --init --recursive --quiet || true
fi

# 1. Project dep cache — pick the right tool based on lockfile / manifest.

# Rust: cargo fetch with --locked so we don't silently mutate Cargo.lock
# in the per-session clone. Stale lockfile produces a non-fatal exit;
# the agent's later cargo build/test surfaces the real issue.
if [ -f Cargo.toml ] && command -v cargo >/dev/null 2>&1; then
  cargo fetch --locked --quiet || true
fi

# Node-based (npm / yarn / pnpm). Skip if node_modules already exists
# (warm from a previous session within the same env-snapshot). Lockless
# package.json (e.g. lockfile gitignored, or simple devDep-only repos)
# falls through to plain `npm install`.
if [ -f package.json ] && [ ! -d node_modules ]; then
  if [ -f package-lock.json ] && command -v npm >/dev/null 2>&1; then
    npm ci 2>/dev/null || npm install
  elif [ -f yarn.lock ] && command -v yarn >/dev/null 2>&1; then
    yarn install --frozen-lockfile 2>/dev/null || yarn install
  elif [ -f pnpm-lock.yaml ] && command -v pnpm >/dev/null 2>&1; then
    pnpm install --frozen-lockfile 2>/dev/null || pnpm install
  elif command -v npm >/dev/null 2>&1; then
    npm install 2>/dev/null || true
  fi
fi

# Ruby / Bundler.
if [ -f Gemfile ] && command -v bundle >/dev/null 2>&1; then
  bundle install --quiet || true
fi

# 2. Pre-commit hook wiring.
# Two flavors: lefthook (binary installed at env-setup time, wired per-repo)
# or a plain repo-local `scripts/pre-commit` script (symlinked into
# .git/hooks/). Errors surface loudly — the whole point of the script is
# the hook install.
if [ -f lefthook.yml ] && command -v lefthook >/dev/null 2>&1; then
  if ! lefthook install; then
    echo "warning: lefthook install failed — pre-commit hook NOT wired" >&2
  fi
elif [ -x scripts/pre-commit ] && [ ! -e .git/hooks/pre-commit ]; then
  if ! ln -sf ../../scripts/pre-commit .git/hooks/pre-commit; then
    echo "warning: pre-commit symlink failed — hook NOT wired" >&2
  fi
fi

exit 0
