#!/usr/bin/env bash
# Regenerate the fork's integration branch from upstream/master + feature list.
#
#   scripts/sync-fork.sh          # dry run: builds locally, does NOT push
#   scripts/sync-fork.sh --push   # force-push fork/main to origin

set -euo pipefail

REMOTE_UPSTREAM="${REMOTE_UPSTREAM:-upstream}"
REMOTE_ORIGIN="${REMOTE_ORIGIN:-origin}"
BASE_BRANCH="${BASE_BRANCH:-master}"
TARGET_BRANCH="${TARGET_BRANCH:-fork/main}"
FEATURES_FILE="${FEATURES_FILE:-scripts/fork-features.txt}"

PUSH=false
if [[ "${1:-}" == "--push" ]]; then
    PUSH=true
fi

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "ERROR: working tree has uncommitted changes — commit or stash first." >&2
    exit 1
fi

# Read features list BEFORE resetting the working tree — otherwise the
# first-ever run can't see the list (it only lands on fork/main after
# the feature/fork-infra merge further down).
if [[ ! -f "$FEATURES_FILE" ]]; then
    echo "ERROR: feature list $FEATURES_FILE not found." >&2
    exit 1
fi

FEATURES=()
while IFS= read -r line; do
    line="${line%%#*}"
    line="$(echo "$line" | xargs)"
    [[ -z "$line" ]] && continue
    FEATURES+=("$line")
done < "$FEATURES_FILE"

echo "Fetching $REMOTE_UPSTREAM/$BASE_BRANCH and $REMOTE_ORIGIN..."
git fetch "$REMOTE_UPSTREAM" "$BASE_BRANCH"
git fetch "$REMOTE_ORIGIN" --prune

echo "Resetting $TARGET_BRANCH to $REMOTE_UPSTREAM/$BASE_BRANCH..."
git checkout -B "$TARGET_BRANCH" "$REMOTE_UPSTREAM/$BASE_BRANCH"

for branch in "${FEATURES[@]}"; do
    remote_ref="$REMOTE_ORIGIN/$branch"
    if ! git show-ref --verify --quiet "refs/remotes/$remote_ref"; then
        echo "  SKIP $branch (not on $REMOTE_ORIGIN)"
        continue
    fi
    if git merge-base --is-ancestor "$remote_ref" HEAD; then
        echo "  SKIP $branch (already in HEAD — probably landed upstream)"
        continue
    fi
    echo "  MERGE $branch"
    if ! git merge --no-ff --no-edit -m "sync: merge $branch" "$remote_ref"; then
        echo "" >&2
        echo "ERROR: conflict merging $branch." >&2
        echo "  Resolve manually (commit the result), then push." >&2
        echo "  Or resolve in the feature branch itself, push it, and rerun." >&2
        exit 1
    fi
done

echo ""
echo "Built $TARGET_BRANCH at $(git rev-parse --short HEAD)."

if $PUSH; then
    echo "Force-pushing to $REMOTE_ORIGIN/$TARGET_BRANCH..."
    git push --force-with-lease "$REMOTE_ORIGIN" "$TARGET_BRANCH:$TARGET_BRANCH"
else
    echo "(dry run — rerun with --push to publish)"
fi
