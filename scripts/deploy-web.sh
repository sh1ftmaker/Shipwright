#!/bin/bash
set -euo pipefail

# Ship of Harkinian - deploy a local web build to GitHub Pages
#
# Builds the web port, then commits the same files the CI workflow publishes
# to the gh-pages branch and pushes it. GitHub Pages serves it a minute or two later.
#
# Usage:
#   scripts/deploy-web.sh [options]
#
# Options:
#   --subdir NAME    Deploy to /NAME/ instead of the site root
#   --no-build       Deploy the existing build outputs without rebuilding
#   --allow-dirty    Deploy even with uncommitted changes
#   --dry-run        Prepare the gh-pages commit but don't push it
#   --remote NAME    Git remote to push to (default: origin)
#
# Requires emsdk activated (emcc/emcmake/emmake in PATH) unless --no-build.

SUBDIR=""
BUILD=1
ALLOW_DIRTY=0
DRY_RUN=0
REMOTE="origin"

while [ $# -gt 0 ]; do
    case "$1" in
        --subdir) SUBDIR="$2"; shift 2 ;;
        --no-build) BUILD=0; shift ;;
        --allow-dirty) ALLOW_DIRTY=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        --remote) REMOTE="$2"; shift 2 ;;
        -h|--help) sed -n '4,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

cd "$(git rev-parse --show-toplevel)"

# What goes live is the working tree, so make sure it matches a commit
if [ "$ALLOW_DIRTY" = 0 ] && [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "Error: uncommitted changes. Commit them first, or pass --allow-dirty." >&2
    exit 1
fi
SHA=$(git rev-parse HEAD)
DIRTY=""
[ -n "$(git status --porcelain --untracked-files=no)" ] && DIRTY=" (with uncommitted changes)"

if [ "$BUILD" = 1 ]; then
    if ! command -v emcmake &> /dev/null; then
        echo "Error: emcmake not found. Activate emsdk first, or pass --no-build." >&2
        exit 1
    fi
    JOBS=$(nproc)

    echo "==> Building soh.o2r"
    [ -f build-tools/build.ninja ] || cmake -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DSOH_TOOLS_ONLY=ON
    cmake --build build-tools --target GenerateSohOtr --parallel "$JOBS"

    echo "==> Building web port"
    [ -f build-web/build.ninja ] || emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
    emmake cmake --build build-web --target soh --parallel "$JOBS"
fi

EXTRA_FLAGS=$(sed -n 's/^CMAKE_EXE_LINKER_FLAGS:STRING=//p' build-web/CMakeCache.txt 2>/dev/null)
if [ -n "$EXTRA_FLAGS" ]; then
    echo "Warning: build-web links with extra flags ($EXTRA_FLAGS), unlike the CI build." >&2
fi

FILES=(build-web/soh/soh.html build-web/soh/soh.js build-web/soh/soh.wasm build-tools/soh/soh.o2r)
for f in "${FILES[@]}"; do
    [ -f "$f" ] || { echo "Error: $f is missing. Build first." >&2; exit 1; }
done

WORKTREE=$(mktemp -d)
cleanup() {
    git worktree remove --force "$WORKTREE" 2>/dev/null || rm -rf "$WORKTREE"
}
trap cleanup EXIT

echo "==> Preparing gh-pages"
git fetch -q "$REMOTE" gh-pages
git worktree add -q --detach "$WORKTREE" FETCH_HEAD

DEST="$WORKTREE/$SUBDIR"
mkdir -p "$DEST"
# Same layout as the CI workflow's "Prepare build output" step
cp build-web/soh/soh.html "$DEST/index.html"
cp build-web/soh/soh.js build-web/soh/soh.wasm "$DEST/"
[ -f build-web/soh/soh.data ] && cp build-web/soh/soh.data "$DEST/"
cp build-tools/soh/soh.o2r "$DEST/"
cp soh/soh/web/soh-save-*.zip "$DEST/"
touch "$WORKTREE/.nojekyll"

git -C "$WORKTREE" add -A
if git -C "$WORKTREE" diff --cached --quiet; then
    echo "gh-pages already has this build. Nothing to deploy."
    exit 0
fi
git -C "$WORKTREE" commit -q -m "deploy: $SHA (local build)$DIRTY"
git -C "$WORKTREE" --no-pager show --stat --format='%h %s' HEAD

if [ "$DRY_RUN" = 1 ]; then
    echo "Dry run: not pushing."
    exit 0
fi

echo "==> Pushing to $REMOTE gh-pages"
git -C "$WORKTREE" push -q "$REMOTE" HEAD:gh-pages

OWNER_REPO=$(git remote get-url "$REMOTE" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')
echo "Deployed. Live in a minute or two at https://${OWNER_REPO%%/*}.github.io/${OWNER_REPO#*/}/${SUBDIR:+$SUBDIR/}"
