# Lumen — Fluent-style KDE theme (working name)

One token source → every engine generated or compiled. Dark + Light + auto. Target Plasma 6.8 / KF 6.31 / Qt 6.12, Wayland.

## Coverage matrix
| Surface | Engine | Source dir | Notes |
|---|---|---|---|
| Qt Widgets apps | C++ QStyle `Lumen` (fork Breeze v6.8.2 kstyle) | `style/` | Fluent controls, 4px radius, elevation border, overlay scrollbars, animations |
| Kirigami / QtQuick apps | same QStyle via qqc2-desktop-style | — | automatic |
| Union (future engine) | generated CSS style | `gen → union/` | optional, ships when Union leaves beta |
| Window frame (SSD) | C++ KDecoration3 `org.lumen.decoration` | `decoration/` | 32px titlebar, 46x32 caption buttons, 8px radius, outline, Win11 shadow, Mica titlebar |
| Window frame (CSD: Firefox, Steam, Discord, Electron) | shadow-only decoration (6.8 feature) | `decoration/` | shadow + outline + rounded corners for all windows |
| Rounded corners + outline | KWin native SDF via decoration `setBorderRadius`/`setBorderOutline` | `decoration/` | zero extra GPU cost; 0px when maximized/tiled |
| Acrylic | stock KWin blur tuned + tint from SVG/scheme alpha | installer config | protocol gives region only; strength global |
| Mica | C++ KWin effect `lumen-mica` (static wallpaper blur cache, screen-UV sampling) | `kwin/effects/` | phase 4; rebuild per KWin patch (pacman hook) |
| Window animations | KWin JS effect (OutQuint ≈ Fluent decel, 167/250ms) | `kwin/effects/` | sets `WindowForceBlurRole` during anim |
| Snap Layouts flyout | C++ effect + QML overlay | `kwin/` | phase 6, no upstream API |
| Plasma shell (panel, dialogs, tooltips, tasks) | KSvg SVG theme `lumen` (generated) | `gen → plasma/desktoptheme/` | follows color scheme + accent; mask-* for blur |
| Taskbar | stock icontasks + panel layout | LnF layout.js | 48px bottom, centered, Win11 pill indicator via `tasks.svg` |
| Start menu | own plasmoid `org.lumen.start` (QML) | `plasma/plasmoids/` | search, pinned grid, recommended, user/power |
| Quick Settings | part of `org.lumen.tray` | `plasma/tray/qml/Qs*.qml` | tiles + sliders; detail pages are the stock applets' own popups (network, bluetooth, volume, battery, brightness) |
| Notification Center + calendar | own plasmoid `org.lumen.clock` | `plasma/plasmoids/` | clock → notifications + calendar flyout |
| System tray | C++ containment `org.lumen.tray` (fork of plasma-workspace 6.8.2 systemtray core + own QML) | `plasma/tray/` | hosts every stock tray plasmoid and SNI item; manual order (`itemOrder`), drag taskbar ↔ overflow, Quick Settings button |
| Alt-Tab, splash | LnF QML | `plasma/look-and-feel/` | |
| Global theme | LnF `org.lumen.{dark,light}` | `plasma/look-and-feel/` | auto day/night via lookandfeelautoswitcher |
| Colors | `.colors` LumenDark/LumenLight (generated) | `gen → colors/` | no hardcoded accent; wallpaper accent |
| GTK3 apps | generated GTK3 CSS theme | `gen → gtk/` | full theme |
| GTK4 / libadwaita | generated `gtk-4.0/gtk.css` named colors + GTK4 theme | `gen → gtk/` | libadwaita: colors only (upstream limit) |
| Firefox | generated `userChrome.css` + GTK colors | `gen → apps/firefox/` | optional install |
| Chromium / Electron | frame only (decoration + materials) | — | apps draw own UI |
| Terminals | generated Konsole / kitty / Alacritty schemes | `gen → apps/terminals/` | |
| Boot menu | Limine `limine.conf` theme (wallpaper, palette, font, branding) | `gen → boot/limine/` | Limine 12.9 |
| Boot / shutdown splash + LUKS prompt | Plymouth script theme `lumen` (logo + Fluent ring spinner, fade) | `boot/plymouth/` | replaces `bgrt`; mkinitcpio `plymouth` hook present |
| TTY console | vconsole palette from tokens | `gen → boot/vconsole/` | |
| Login | **open:** Plasma Login Manager (wallpaper + our Plasma theme/colors only) **or** switch to SDDM + own QML greeter (full control) | `plasma/login/` | PLM greeter QML is compiled in |
| Plasma splash (after login) | LnF `splash/Splash.qml` | LnF | |
| Lock screen | shell-package overlay (`/usr/local/share/plasma/shells/org.kde.plasma.desktop`) | `plasma/shell-overlay/` | 6.8 loads it from the shell package; stock logic kept, Fluent password block; Lumen wallpaper |
| Logout / shutdown dialog | shell-package overlay | `plasma/shell-overlay/` | Fluent tiles; refreshed by pacman hook |
| OSD (volume, brightness, layout) | Plasma style SVGs | generated | hard-wired QML module in 6.8 (`org.kde.plasma.workspace.osd`); themed via dialog/progress SVGs |
| KRunner, notifications popups, polkit dialog | Plasma SVG theme + QStyle | — | follow automatically |
| Event sounds | own freedesktop sound theme `lumen` (optional) | `sounds/` | Win sounds proprietary → original sounds only |
| Icons / cursors / font | never set: the icon and cursor themes the user assigned stay / Segoe UI Variable | — | Segoe not bundled |

## Repo layout
```
tokens/        tokens.toml — single source (colors dark/light, radii, sizes, motion, materials, shadows)
tools/lumen/   Rust CLI: `lumen gen` (tokens → assets), `lumen install`, `lumen rebuild` (after KWin updates)
style/         C++ QStyle
decoration/    C++ KDecoration3
kwin/          C++ effect + JS effects
plasma/        desktoptheme (gen), look-and-feel, plasmoids
gtk/ apps/ colors/ union/   generated (gitignored build output under build/)
research/      findings
```

## Phases
1. **Foundation** — tokens.toml, Rust generator, color schemes, Plasma SVG theme, LnF + panel layout, GTK3/4, installer. Replaces: KlassyDark, Plasma theme, panel-colorizer, spacers, Adapta.
2. **Decoration** — KDecoration3 (+ shadow-only CSD mode). Replaces Klassy decoration.
3. **QStyle** — Fluent controls. Replaces Klassy style, Kvantum.
4. **KWin** — materials effect + animations. Replaces glass effect.
5. **Shell + session** — Start, Quick Settings, Notification Center, Alt-Tab, OSD, splash, lock, logout, login.
6. **Boot** — Plymouth, Limine, TTY palette (system files under /boot, /usr → installer asks sudo).
7. **Extras** — Firefox, terminals, sounds, Union CSS, Snap Layouts, benchmarks.

Each phase: build → install → benchmark (paint time / frame time) → visual check.
