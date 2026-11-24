# Lumen for KDE Plasma

[![CI](https://github.com/dzcorp/kdewin/actions/workflows/ci.yml/badge.svg)](https://github.com/dzcorp/kdewin/actions/workflows/ci.yml)
[![Upstream sync](https://github.com/dzcorp/kdewin/actions/workflows/upstream-sync.yml/badge.svg)](https://github.com/dzcorp/kdewin/actions/workflows/upstream-sync.yml)
[![AUR](https://img.shields.io/aur/version/lumen-kde-git?label=AUR%20lumen-kde-git)](https://aur.archlinux.org/packages/lumen-kde-git)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)](LICENSE)

A Windows 11 style desktop for KDE Plasma 6.8, built as one system rather than a stack of separate themes and
plugins. Every part (Qt style, window decoration, KWin effects, Plasma theme, GTK, boot splash) is generated or
compiled from one design-token file, `tokens/tokens.toml`. Light, dark and automatic modes; the accent color follows Plasma.

> Lumen is not affiliated with or endorsed by Microsoft. No Microsoft assets are included; see [Fonts](#fonts).

## What you get

| Area | Parts |
|---|---|
| Apps | Qt widget style (C++), Qt Quick Controls and Kirigami styles, GTK 3/4 and libadwaita colors, Union style, Konsole/kitty/Alacritty schemes, Firefox userChrome |
| Windows | Window decoration (KDecoration3), Mica and Acrylic materials (KWin C++ effect, replaces stock blur), open/close and minimize animations |
| Shell | Centered 48 px taskbar, Start menu, Quick Settings in the tray, clock with Notification Center and persistent history, Windows-style toasts |
| File Explorer | Dolphin fork with tabs in the title bar, address-row search, This PC page, navigation pane sections |
| System | Lock and logout screens, splash, OSD, Alt-Tab, login screen (Plasma Login Manager), Plymouth boot splash, Limine boot menu, console palette |
| Settings | System Settings → Appearance → **Lumen**: mode, materials, windows, app style, animations, taskbar, lock/login, boot, maintenance (changes apply live) |

Your icon theme, cursor theme and wallpaper are never changed. The `windows-modern` icon theme is installed but not selected.

## Requirements

Arch Linux or a derivative (CachyOS, EndeavourOS, Manjaro…) with KDE Plasma 6.8 or newer, Qt 6.9+, KDE Frameworks 6.20+,
on Wayland or X11. The native plugins are built against the installed KWin, Qt and Plasma, so they must be rebuilt after
those update (see [Updates](#updates)).

## Fonts

Windows 11 uses Segoe UI Variable, Segoe UI and Consolas. Their Microsoft licence doesn't allow redistribution, so
Lumen never ships or depends on them. `lumen apply` uses the first font that is installed:

| Role | Preferred | Fallbacks |
|---|---|---|
| Interface | Segoe UI Variable | Selawik, Inter (`inter-font`, installed with the package), Noto Sans |
| Title bars | Segoe UI | interface font |
| Monospace | Consolas | Liberation Mono (`ttf-liberation`, installed with the package) |

To get the Windows fonts, if you hold a Windows licence:

- `lumen fonts /path/to/windows-partition` copies them from your own Windows install, or
- `yay -S ttf-ms-win11-auto` extracts them from Microsoft's Windows 11 evaluation ISO (a large download).

`ttf-selawik` (AUR) is Microsoft's open-source Segoe UI look-alike. `ttf-segoe-ui-variable` is not suggested: its
licence allows use only for designing and testing programs for Microsoft platforms. Run `lumen apply` again after
installing fonts.

## Install

### From the AUR (yay or paru)

```sh
yay -S lumen-kde-git          # or: paru -S lumen-kde-git
lumen apply auto --layout     # in your Plasma session; drop --layout to keep your panel as is
lumen login on                # optional: Lumen on the login screen
lumen boot on                 # optional: Plymouth boot splash and Limine boot menu
```

Log out and back in afterwards. `lumen-kde` (tagged releases) is published alongside `lumen-kde-git`.

### With the install script

```sh
git clone https://github.com/dzcorp/kdewin.git
cd kdewin
./install.sh                  # asks about the taskbar, login screen and boot theme
```

The script builds a `lumen-kde-git` pacman package from your checkout (or installs it from the AUR with `--aur`),
so pacman tracks every file. Then it applies Lumen and runs `lumen doctor`. Options:

```
--aur                    install lumen-kde-git from the AUR instead of building this checkout
--mode dark|light|auto   color mode (default auto: follows Plasma's light/dark switching)
--layout, --no-layout    put the Lumen taskbar on the bottom panel (keeps your widgets and pinned apps)
--login, --no-login      Lumen on the login screen
--boot, --no-boot        Plymouth boot splash and Limine boot menu (rebuilds the initramfs)
-y, --yes                no questions: taskbar and login screen on, boot theme only with --boot
```

## Uninstall

```sh
./uninstall.sh
```

or by hand:

```sh
lumen uninstall               # back to Breeze: global theme, decoration, effects, fonts, GTK; panel widgets swapped in place
sudo pacman -Rns lumen-kde-git   # also restores the previous boot splash, boot menu and login screen
```

Log out and back in to finish. Your Lumen settings (`~/.config/lumenrc`) are kept for a later reinstall.

## Usage

| Command | What it does |
|---|---|
| `lumen apply [dark\|light\|auto] [--layout]` | Activate Lumen; `--layout` also installs the Lumen taskbar and keeps your panel widgets, pinned apps and tray settings |
| `lumen doctor` | Check that everything is installed and loaded; falls back to stock blur if the effect did not load |
| `lumen restart` | Refresh icons, the desktop shell and search after icon or theme changes made elsewhere |
| `lumen login on\|off`, `lumen boot on\|off` | Login screen; boot splash and boot menu (asks for your password) |
| `lumen fonts <windows-partition>` | Import Segoe UI, Segoe UI Variable, Segoe Fluent Icons and Consolas from your own Windows install |
| `lumen cleanup` | List (and with confirmation remove) packages Lumen replaces, such as Kvantum or Klassy |
| `lumen snapshot` | Show what `apply --layout` would carry over from the current panel |

Settings live in `~/.config/lumenrc` and are edited in System Settings → Appearance → Lumen. The style and decoration
reload them live.

## Updates

- **AUR package:** after a KWin, Qt or Plasma update pacman prints a reminder: rebuild with `yay -S lumen-kde-git`.
  Until then `lumen doctor` switches the effects back to stock blur. The lock and logout screen overlay is refreshed
  automatically by a pacman hook.
- **Source install** (`make install-system`): pacman hooks rebuild the native plugins from your checkout automatically.

## Forked KDE components and automatic upstream merges

Some parts are forks of KDE code, because Plasma gives no way to change them from outside:

| Fork | Upstream | Path |
|---|---|---|
| Tray (with Quick Settings) | plasma-workspace `applets/systemtray`, `libdbusmenuqt` | `plasma/tray/src`, `plasma/tray/dbusmenuqt` (QML rewritten) |
| Notifications | plasma-workspace `applets/notifications` | `plasma/notifier` |
| Taskbar apps | plasma-desktop `applets/taskmanager` | `plasma/tasks` |
| File Explorer | dolphin `src` | `apps/dolphin` |

`upstream/forks.toml` records the upstream release each fork is based on. `lumen upstream sync` downloads that release and
the new one from download.kde.org and merges every file three-way. Files we never changed take the upstream version.
Files both sides changed are merged with `git merge-file`. New upstream files are added, and files upstream removed are
deleted when we did not change them.

The **Upstream sync** workflow runs this daily against the version Arch ships. If the merge is clean and the result
builds and passes the checks, it opens a pull request and merges it. Conflicts or build failures leave the pull request
open with the `needs-attention` label and a report of every file.

```sh
make upstream-check                 # current vs newest release per fork
make upstream-sync                  # merge all forks locally
make upstream-sync FORK=dolphin TO=26.08.3
```

## Build from source (development)

Needs Rust, CMake, Ninja, extra-cmake-modules and the KDE development packages listed in
[`packaging/aur/lumen-kde-git/PKGBUILD`](packaging/aur/lumen-kde-git/PKGBUILD).

```sh
make                    # Rust tool + generated assets + native plugins + Dolphin fork
make install-system     # install into /usr with sudo (files outside pacman; plugins rebuilt by pacman hooks)
make apply MODE=auto    # or: make apply-layout
make doctor
make check-tray         # Quick Settings tray icon drag, click and ordering checks
make check-settings     # settings module persistence and live updates (isolated D-Bus session)
make bench              # generator timings and widget style paint benchmark vs Breeze/Fusion
make kcm                # open the settings module from the build tree
make uninstall-system
```

A source install and the package can't be installed at the same time: run `./uninstall.sh` before switching between them.

### Layout

| Path | Contents |
|---|---|
| `tokens/tokens.toml` | Design tokens: colors, type ramp, sizes, motion, materials |
| `tools/lumen/` | Rust CLI: generator, installer, `apply`, `doctor`, upstream merge |
| `style/`, `decoration/`, `kwin/`, `common/` | Qt style, window decoration, KWin effects, shared C++ |
| `plasma/` | Plasmoids, tray, taskbar, notifications, look-and-feel packages, shell overlay |
| `apps/dolphin/` | File Explorer (Dolphin fork) |
| `kcm/` | System Settings module |
| `qqc2/`, `kirigami-style/`, `kirigami-shadow/` | Qt Quick Controls style, Kirigami style, Kirigami forms patch |
| `po/` | Translations (20 languages) |
| `packaging/aur/` | PKGBUILDs for `lumen-kde-git` and `lumen-kde` |
| `upstream/forks.toml` | Forked upstream sources and their versions |
| `research/` | Design and API notes (Windows 11 metrics, KDE theming APIs) |

More detail: [ARCHITECTURE.md](ARCHITECTURE.md).

## CI/CD

| Workflow | When | What |
|---|---|---|
| `ci.yml` | push, pull request | shellcheck; full build in an Arch container with tray checks and generator benchmark; builds the pacman package, runs namcap, installs and removes it (hooks included) |
| `upstream-sync.yml` | daily, manual | three-way merge of the forks with the release Arch ships, build, pull request, automatic merge when clean |
| `aur.yml` | `v*` tag only | GitHub release; publishes `lumen-kde` (that version) and `lumen-kde-git` to the AUR. Commits to main never publish |

Repository secrets:

- `AUR_SSH_PRIVATE_KEY`: SSH key of the AUR account that maintains the packages. AUR publishing is skipped without it.
- `SYNC_TOKEN` (optional): a fine-grained token with *contents* and *pull requests* write access. Without it the sync's
  pull requests are created with the workflow token, which starts no other workflows (the sync job builds and checks
  the merge itself).

Release: bump the version in `tools/lumen/Cargo.toml`, `CMakeLists.txt` and `tokens/tokens.toml`, then
`git tag vX.Y.Z && git push --tags`.

## Contributing

Issues and pull requests are welcome. Keep changes small and focused, run `make` and `make check-tray` before
opening a pull request, and derive new sizes and colors from `tokens/tokens.toml` rather than hard-coding them.
Changes to the forks should stay minimal, so upstream merges stay clean.

## License

Lumen is free software under the [GNU General Public License v3.0 or later](LICENSE).
The forked KDE sources keep their own licenses: GPL-2.0-or-later (tray, taskbar, notifications), LGPL-2.0-or-later
(`libdbusmenuqt`), and Dolphin's GPL-2.0-or-later / KDE-accepted GPL ([`apps/dolphin/LICENSES`](apps/dolphin/LICENSES)).
The `windows-modern` icon theme (GPL-3.0) comes from [KDE-Windows-Modern](https://github.com/Jeysef/KDE-Windows-Modern);
see [`icons/UPSTREAM`](icons/UPSTREAM) for the packs it draws on.
