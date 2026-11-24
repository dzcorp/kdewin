#!/usr/bin/env bash
# Lumen uninstaller: switches the session back to Breeze (panel widgets swapped in place), then removes the
# package, or the system files of a source install (make install-system). Your icon and cursor themes stay.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
yes=0
case ${1:-} in
-y | --yes) yes=1 ;;
-h | --help)
	echo "Usage: ./uninstall.sh [-y]   (-y: remove the package without asking)"
	exit 0
	;;
"") ;;
*)
	echo "Usage: ./uninstall.sh [-y]" >&2
	exit 2
	;;
esac

say() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

((EUID != 0)) || die "run as your user, not root (sudo is asked for where needed)"
pkg=$(pacman -Qq lumen-kde-git lumen-kde 2>/dev/null | head -1 || true)
noconfirm=()
((yes)) && noconfirm=(--noconfirm)

if [[ -n $pkg ]]; then
	say "Switching this session back to Breeze"
	lumen uninstall
	say "Removing the $pkg package (boot and login themes are restored first)"
	sudo pacman -Rns "${noconfirm[@]}" "$pkg"
elif [[ -x /usr/local/bin/lumen ]]; then
	root=()
	[[ -f $here/tokens/tokens.toml ]] && root=(--root "$here")
	say "Switching this session back to Breeze and removing the source install"
	/usr/local/bin/lumen "${root[@]}" uninstall --system
elif command -v lumen >/dev/null; then
	lumen uninstall
else
	die "Lumen is not installed"
fi
say "Done. Log out and back in to finish (KWin unloads Lumen's plugins on the next login)."
