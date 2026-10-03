#!/usr/bin/env bash
#
# Runs clang-tidy (.clang-tidy at the root) on the headers of the given modules, each header parsed on its own.
#
#   tools/clang-tidy.sh [module...]     default: the modules listed below
#
# Legacy modules (common, events, ioc, imdb, maths) are not checked yet: their headers don't build on their own.
#
set -euo pipefail

cd "$(dirname "$0")/.."

CLANG_TIDY=${CLANG_TIDY:-clang-tidy-18}
modules=("$@")

if [ ${#modules[@]} -eq 0 ]; then
  modules=(async webkit)
fi

flags=(-x c++ -std=c++20 -fPIC -DASIO_STANDALONE)

for dir in puffin/*/include; do
  flags+=("-I$dir")
done

# Optional dependencies of the adapters
if pkg-config --exists Qt6Network 2>/dev/null; then
  read -ra qt <<< "$(pkg-config --cflags Qt6Network)"
  flags+=("${qt[@]}")
else
  echo "warning: Qt6Network not found by pkg-config, the Qt adapter headers will fail to parse" >&2
fi

headers=()

for module in "${modules[@]}"; do
  while IFS= read -r header; do
    headers+=("$header")
  done < <(find "puffin/$module/include" -name '*.hpp' | sort)
done

status=0

for header in "${headers[@]}"; do
  echo "clang-tidy $header"
  # Only the header under check: the ones it includes get their own run
  "$CLANG_TIDY" --quiet --header-filter='^$' "$header" -- "${flags[@]}" || status=1
done

exit $status
