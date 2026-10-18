#!/usr/bin/env bash
# OpenGT4 Android - keep two repositories on the same commit.
#
#   private : Star1xr/opengt4privaterepo   (builds, private-assets branch)
#   public  : Star1xr/OpenGT4-Android      (same commits, without private assets)
#
# Usage:
#   tools/sync_repos.sh --setup                 # create the remotes (no token stored)
#   GITHUB_TOKEN=xxx tools/sync_repos.sh        # push main to both repositories
#   GITHUB_TOKEN=xxx tools/sync_repos.sh --only private
#   GITHUB_TOKEN=xxx tools/sync_repos.sh --tag v0.3.0-android.1
#
# The token is never written to a file or into a remote URL: it is used once as
# an http.extraheader on the push command line.
set -euo pipefail

PRIVATE_REPO="Star1xr/opengt4privaterepo"
PUBLIC_REPO="Star1xr/OpenGT4-Android"
PRIVATE_REMOTE="private"
PUBLIC_REMOTE="public"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"

ONLY=""
TAG=""
SETUP=0
while [ $# -gt 0 ]; do
    case "$1" in
        --setup) SETUP=1 ;;
        --only) shift; ONLY="${1:-}" ;;
        --tag) shift; TAG="${1:-}" ;;
        -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
    shift
done

cd "$(git rev-parse --show-toplevel)"

setup_remotes() {
    git remote add "$PRIVATE_REMOTE" "https://github.com/${PRIVATE_REPO}.git" 2>/dev/null || \
        git remote set-url "$PRIVATE_REMOTE" "https://github.com/${PRIVATE_REPO}.git"
    git remote add "$PUBLIC_REMOTE" "https://github.com/${PUBLIC_REPO}.git" 2>/dev/null || \
        git remote set-url "$PUBLIC_REMOTE" "https://github.com/${PUBLIC_REPO}.git"
    echo "remotes ready:"
    git remote -v | grep -E "^(${PRIVATE_REMOTE}|${PUBLIC_REMOTE})\b"
}

if [ "$SETUP" = "1" ]; then
    setup_remotes
    exit 0
fi

# --- Local-only files and game data must never reach the repositories --------
leak_guard() {
    local bad
    bad="$(git ls-files | grep -E '(^|/)(AGENTS\.md|opengt4-android-plan\.md)$|^\.opencode/' || true)"
    if [ -n "$bad" ]; then
        echo "ERROR: local-only files are tracked (they must not be pushed):" >&2
        echo "$bad" >&2
        exit 1
    fi
    bad="$(git status --porcelain | awk '{print $NF}' | grep -E '\.(iso|bin|chd|cue|img|jks|keystore|pem)$' || true)"
    if [ -n "$bad" ]; then
        echo "ERROR: game data or keys appear to be staged:" >&2
        echo "$bad" >&2
        exit 1
    fi
    if git grep -qE 'ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}' -- . ':!tools/sync_repos.sh' 2>/dev/null; then
        echo "ERROR: a token-like string is present in the committed content." >&2
        exit 1
    fi
}

push_one() {
    local remote="$1"
    echo "==> ${remote} (${BRANCH})"
    git -c "http.extraheader=${AUTH_HEADER}" push "$remote" "refs/heads/${BRANCH}:refs/heads/${BRANCH}"
    if [ -n "$TAG" ]; then
        git tag -f "$TAG" >/dev/null 2>&1 || true
        git -c "http.extraheader=${AUTH_HEADER}" push "$remote" "refs/tags/${TAG}:refs/tags/${TAG}" --force
    fi
}

if [ -z "${GITHUB_TOKEN:-}" ]; then
    echo "GITHUB_TOKEN is not set. For example:" >&2
    echo "  GITHUB_TOKEN=xxx tools/sync_repos.sh" >&2
    exit 2
fi

leak_guard
setup_remotes

# basic x-access-token:<token> - the form GitHub's smart HTTP expects
AUTH_HEADER="AUTHORIZATION: basic $(printf 'x-access-token:%s' "$GITHUB_TOKEN" | base64 | tr -d '\n')"

case "$ONLY" in
    private) push_one "$PRIVATE_REMOTE" ;;
    public)  push_one "$PUBLIC_REMOTE" ;;
    "")      push_one "$PRIVATE_REMOTE"; push_one "$PUBLIC_REMOTE" ;;
    *)       echo "unknown --only value: $ONLY" >&2; exit 2 ;;
esac

echo "done: ${BRANCH}${TAG:+ + tag ${TAG}} pushed."
