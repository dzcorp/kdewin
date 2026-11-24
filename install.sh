#!/usr/bin/env bash
# Lumen installer for Arch Linux and derivatives: installs the pacman package (built from this checkout, or
# lumen-kde-git from the AUR) and applies Lumen to the running Plasma session. Undo with ./uninstall.sh.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
mode=auto layout=ask login=ask boot=ask yes=0 from=local

usage() {
	cat <<'EOF'
Usage: ./install.sh [options]

  --aur                    install lumen-kde-git from the AUR (yay or paru) instead of building this checkout
  --mode dark|light|auto   color mode (default auto: follows Plasma's light/dark switching)
  --layout, --no-layout    put the Lumen taskbar on the bottom panel (keeps your widgets and pinned apps)
  --login, --no-login      Lumen on the login screen (Plasma Login Manager)
  --boot, --no-boot        Plymouth boot splash and Limine boot menu (rebuilds the initramfs)
  -y, --yes                no questions: taskbar and login screen on, boot theme only with --boot
  -h, --help
EOF
}

while (($#)); do
	case $1 in
	--aur) from=aur ;;
	--mode)
		[[ ${2:-} =~ ^(dark|light|auto)$ ]] || { usage >&2; exit 2; }
		mode=$2
		shift
		;;
	--layout) layout=y ;;
	--no-layout) layout=n ;;
	--login) login=y ;;
	--no-login) login=n ;;
	--boot) boot=y ;;
	--no-boot) boot=n ;;
	-y | --yes) yes=1 ;;
	-h | --help) usage; exit 0 ;;
	*) usage >&2; exit 2 ;;
	esac
	shift
done

say() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
die() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

# ask VAR QUESTION DEFAULT(y|n): leaves answers given on the command line alone.
ask() {
	local -n answer=$1
	local reply
	[[ $answer == ask ]] || return 0
	if ((yes)) || [[ ! -t 0 ]]; then
		answer=$3
		return 0
	fi
	read -rp "$2 [$([[ $3 == y ]] && echo Y/n || echo y/N)] " reply
	[[ ${reply:-$3} == [Yy]* ]] && answer=y || answer=n
}

((EUID != 0)) || die "run as your user, not root (sudo is asked for where needed)"
command -v pacman >/dev/null || die "pacman not found: this installer is for Arch Linux and derivatives"
[[ ! -x /usr/local/bin/lumen ]] || die "a source install (make install-system) is present: run ./uninstall.sh first, or keep updating it with make install-system"

if plasma=$(plasmashell --version 2>/dev/null | awk '{print $2}') && [[ -n $plasma ]]; then
	[[ $(printf '%s\n' 6.8 "$plasma" | sort -V | head -1) == 6.8 ]] || warn "Plasma $plasma found; Lumen is made for Plasma 6.8 and newer"
else
	warn "plasmashell not found: Lumen needs KDE Plasma 6.8"
fi

ask layout "Put the Lumen taskbar on the bottom panel?" y
ask login "Use Lumen on the login screen?" y
ask boot "Install the boot splash and boot menu theme (rebuilds the initramfs)?" n
noconfirm=()
((yes)) && noconfirm=(--noconfirm)

if [[ $from == aur ]]; then
	helper=$(command -v yay || command -v paru) || die "--aur needs yay or paru"
	say "Installing lumen-kde-git from the AUR with ${helper##*/}"
	"$helper" -S --needed "${noconfirm[@]}" lumen-kde-git
else
	git -C "$here" rev-parse --verify -q HEAD >/dev/null || die "$here is not a git checkout: clone the repository, or use --aur"
	[[ -z $(git -C "$here" status --porcelain --untracked-files=no) ]] || warn "uncommitted changes are not built: the package uses the committed HEAD"
	pacman -Qq base-devel >/dev/null 2>&1 || sudo pacman -S --needed "${noconfirm[@]}" base-devel git
	work=$(mktemp -d)
	trap 'rm -rf "$work"' EXIT
	cp -L "$here"/packaging/aur/lumen-kde-git/* "$work/"
	sed -i "s|^source=.*|source=(\"\$_repo::git+file://$here#commit=$(git -C "$here" rev-parse HEAD)\")|" "$work/PKGBUILD"
	say "Building the lumen-kde-git package from $here (this compiles Lumen, a few minutes)"
	(cd "$work" && makepkg --syncdeps --install --needed "${noconfirm[@]}")
fi

command -v lumen >/dev/null || die "lumen is not on PATH after the install"
if [[ -z ${WAYLAND_DISPLAY:-}${DISPLAY:-} ]] || ! qdbus6 org.kde.plasmashell /PlasmaShell >/dev/null 2>&1; then
	say "Installed. Log in to Plasma, then run: lumen apply $mode$([[ $layout == y ]] && echo ' --layout')"
	exit 0
fi

args=(apply "$mode")
[[ $layout == y ]] && args+=(--layout)
say "Applying Lumen ($mode)"
lumen "${args[@]}"
if [[ $login == y ]]; then
	say "Lumen login screen"
	lumen login on
fi
if [[ $boot == y ]]; then
	say "Boot splash and boot menu"
	lumen boot on
fi
lumen doctor || warn "some checks failed: KWin loads the new plugins after you log out and back in"
say "Done. Log out and back in so KWin and the session pick up everything."
