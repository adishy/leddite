#!/usr/bin/env bash
# Fails if any tracked file is a compiled binary.
#
# Committed build artifacts have broken this repo before: the test binaries were
# committed as arm64 Mach-O executables, so `make test` died with "Exec format
# error" on Linux while make considered them up to date. See RULES.md.
#
# Usage:
#   tools/check-no-binaries.sh            # check tracked files (HEAD)
#   tools/check-no-binaries.sh --staged   # check what is about to be committed
set -uo pipefail

MODE="${1:-tracked}"

# simulator/leddite_wasm.* is a deliberate, documented exception (RULES.md §1).
ALLOWED_RE='^simulator/leddite_wasm\.(js|wasm)$'

# Sources that happen to be binary formats — nothing in this repo generates them.
SOURCE_RE='\.(svg|pdf|png|jpg|jpeg|gif|ico|ttf|woff2?)$'

if [ "$MODE" = "--staged" ]; then
  FILES=$(git diff --cached --name-only --diff-filter=ACM)
else
  FILES=$(git ls-files)
fi

BAD=()
while IFS= read -r f; do
  [ -z "$f" ] || [ ! -f "$f" ] && continue
  [[ "$f" =~ $ALLOWED_RE ]] && continue
  [[ "$f" =~ $SOURCE_RE ]] && continue

  case "$(file -b --mime-type "$f")" in
    application/x-executable|application/x-sharedlib|application/x-mach-binary|application/x-pie-executable|application/x-object|application/x-archive)
      BAD+=("$f")
      ;;
  esac
done <<< "$FILES"

if [ "${#BAD[@]}" -gt 0 ]; then
  echo "ERROR: compiled binaries must not be committed (RULES.md §1):" >&2
  for f in "${BAD[@]}"; do
    echo "  $f  [$(file -b --mime-type "$f")]" >&2
  done
  echo "" >&2
  echo "Add them to .gitignore and 'git rm --cached <file>'." >&2
  exit 1
fi

echo "check-artifacts: no compiled binaries tracked"
