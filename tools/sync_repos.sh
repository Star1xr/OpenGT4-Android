#!/usr/bin/env bash
# OpenGT4 Android - iki repoyu ayni commit ile guncel tutar.
#
#   private : Star1xr/opengt4privaterepo   (CI build'leri, private-assets branch'i)
#   public  : Star1xr/OpenGT4-Android      (ayni commit'ler, ozel assetler olmadan)
#
# Kullanim:
#   tools/sync_repos.sh --setup                 # remote'lari tanimlar (token yazmaz)
#   GITHUB_TOKEN=xxx tools/sync_repos.sh        # main'i iki repoya da push eder
#   GITHUB_TOKEN=xxx tools/sync_repos.sh --only private
#   GITHUB_TOKEN=xxx tools/sync_repos.sh --tag v0.3.0-android.1
#
# Token asla dosyaya/remote URL'ine yazilmaz: tek kullanimlik http.extraheader.
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
        *) echo "bilinmeyen arguman: $1" >&2; exit 2 ;;
    esac
    shift
done

cd "$(git rev-parse --show-toplevel)"

setup_remotes() {
    git remote add "$PRIVATE_REMOTE" "https://github.com/${PRIVATE_REPO}.git" 2>/dev/null || \
        git remote set-url "$PRIVATE_REMOTE" "https://github.com/${PRIVATE_REPO}.git"
    git remote add "$PUBLIC_REMOTE" "https://github.com/${PUBLIC_REPO}.git" 2>/dev/null || \
        git remote set-url "$PUBLIC_REMOTE" "https://github.com/${PUBLIC_REPO}.git"
    echo "remote'lar hazir:"
    git remote -v | grep -E "^(${PRIVATE_REMOTE}|${PUBLIC_REMOTE})\b"
}

if [ "$SETUP" = "1" ]; then
    setup_remotes
    exit 0
fi

# --- Yerel (push edilmeyecek) dosyalar ve oyun verisi asla git'e girmez -------
leak_guard() {
    local bad
    bad="$(git ls-files | grep -E '(^|/)(AGENTS\.md|opengt4-android-plan\.md)$|^\.opencode/' || true)"
    if [ -n "$bad" ]; then
        echo "HATA: yerel dosyalar takip ediliyor (push edilmez):" >&2
        echo "$bad" >&2
        exit 1
    fi
    bad="$(git status --porcelain | awk '{print $NF}' | grep -E '\.(iso|bin|chd|cue|img|jks|keystore|pem)$' || true)"
    if [ -n "$bad" ]; then
        echo "HATA: oyun verisi/anahtar staged gorunuyor:" >&2
        echo "$bad" >&2
        exit 1
    fi
    if git grep -qE 'ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}' -- . ':!tools/sync_repos.sh' 2>/dev/null; then
        echo "HATA: commit iceriginde token benzeri dizi var." >&2
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
    echo "GITHUB_TOKEN tanimli degil. Ornek:" >&2
    echo "  GITHUB_TOKEN=xxx tools/sync_repos.sh" >&2
    exit 2
fi

leak_guard
setup_remotes

# basic x-access-token:<token>  - GitHub smart HTTP'nin bekledigi bicim
AUTH_HEADER="AUTHORIZATION: basic $(printf 'x-access-token:%s' "$GITHUB_TOKEN" | base64 | tr -d '\n')"

case "$ONLY" in
    private) push_one "$PRIVATE_REMOTE" ;;
    public)  push_one "$PUBLIC_REMOTE" ;;
    "")      push_one "$PRIVATE_REMOTE"; push_one "$PUBLIC_REMOTE" ;;
    *)       echo "bilinmeyen --only degeri: $ONLY" >&2; exit 2 ;;
esac

echo "tamam: ${BRANCH}${TAG:+ + tag ${TAG}} gonderildi."
