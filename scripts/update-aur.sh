#!/bin/bash
set -euo pipefail

usage() {
  cat >&2 <<EOF
Usage: $0 <version>

Updates the AUR package to <version>.

Requires:
  - SSH key in ~/.ssh/aur (or AUR_SSH_KEY env var) registered on aur.archlinux.org
  - AUR_PACKAGE env var (e.g. "reverso-linux") or -p flag

Example:
  AUR_PACKAGE=reverso-linux ./scripts/update-aur.sh 1.2.3
EOF
  exit 1
}

if [ $# -lt 1 ]; then
  usage
fi

VERSION="$1"
AUR_PACKAGE="${AUR_PACKAGE:-reverso-linux}"
REPO_URL="ssh://aur@aur.archlinux.org/$AUR_PACKAGE.git"
GH_REPO="${GITHUB_REPOSITORY:-YOUR_USERNAME/reverso-linux}"

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

if [ -n "${AUR_SSH_KEY:-}" ]; then
  mkdir -p ~/.ssh
  ssh-keyscan -H aur.archlinux.org >> ~/.ssh/known_hosts 2>/dev/null
  echo "$AUR_SSH_KEY" > ~/.ssh/aur
  chmod 600 ~/.ssh/aur
fi

cd "$TMPDIR"
git clone -q "$REPO_URL" aur-pkg
cd aur-pkg

curl -sL "https://github.com/$GH_REPO/archive/refs/tags/v$VERSION.tar.gz" \
  -o "reverso-linux-v$VERSION.tar.gz"

SUM=$(sha256sum "reverso-linux-v$VERSION.tar.gz" | cut -d' ' -f1)
rm -f "reverso-linux-v$VERSION.tar.gz"

sed -i "s/^pkgver=.*/pkgver=$VERSION/" PKGBUILD
sed -i "s/^pkgrel=.*/pkgrel=1/" PKGBUILD
sed -i "s/^sha256sums=.*/sha256sums=('$SUM')/" PKGBUILD

makepkg --printsrcinfo > .SRCINFO

git add PKGBUILD .SRCINFO
git diff --staged --quiet || {
  git -c user.name="reverso-linux release" \
      -c user.email="releases@reverso-linux" \
      commit -m "Release v$VERSION"
  git push
  echo "Pushed $AUR_PACKAGE v$VERSION to AUR"
}
