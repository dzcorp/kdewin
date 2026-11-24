# Local KDE look & feel inventory (2026-11-02)

System: Plasma 6.8.2, KF 6.31, Qt 6.12, Wayland, NVIDIA RTX 4080, single output DP-2 2560x1440@144Hz scale 1.0.

## Active stack
| Part | Provider | Notes |
|---|---|---|
| Global theme | `klassy-custom` (~/.local/share/plasma/look-and-feel) | defaults + stale layout.js snapshot |
| Widget style | Klassy 6.7.3 (`klassy6.so`) | `ButtonGradient=true` |
| Decoration | Klassy | 4px radius, integrated rounded buttons, Material-dynamic icons, titlebar opacity 50/30 + blur, contrast outline 50 |
| Colors | `/usr/share/color-schemes/KlassyDark.colors` | = Breeze Dark, `[WM] activeBackground=70,75,81,191`, contrast 4, frameContrast 0.2 |
| Plasma style | `Plasma` (Breeze/Darkly fork, API 5.0) | ~16 SVGs: panel/dialog/tooltip translucent w/ masks, opacity 0.85 |
| Icons | YAMIS | keep |
| Cursor | capitaine-cursors 24 | keep |
| Fonts | Segoe UI Variable Static Text 11; clock Small Semilight 14 | `ttf-segoe-ui-variable`; mono Liberation Mono 11 |
| GTK | Adapta + kde-gtk-config colors.css | no libadwaita override |
| Kvantum | KvAdaptaDark | **not applied** (unused) |
| KWin blur | stock, strength 5, noise 13, saturation 289 | |
| KWin glass | kwin-effects-glass-git | **broken**: built for 6.6.4, not loading on 6.8.2 |
| SDDM / splash / lock | stock breeze / none | |

## Panel 23
Bottom, 44px, fill, non-floating, translucent. Order: kurve, spacer, kickoff (`cachyos` icon, Alt+F1), pager, icontasks, spacer, systemtray, marginsseparator, apdatifier, digitalclock (`ddd d MMM`, date below), brightness, panel-colorizer.

- Panel Colorizer: only produces flat black bar @65% alpha, hides native bg, white fg, spacing 4, 1password tray icon swap. No blur/radius.
- panelspacer.extended: two expanding spacers → centered tasks. Stock `org.kde.plasma.panelspacer` suffices.
- Kurve visualizer: 1000px, 144fps, hide when idle — keep as optional.
- Systemtray shown: kdeconnect, bluetooth, cameraindicator, devicenotifier, inputmethod, mediacontroller, notifications, keyboardindicator, weather, kscreen, battery, keyboardlayout, network, volume, dwellclicker. Hidden: EasyEffects, brightness.
- Icontasks pins: systemsettings, dolphin, browser.

Unused plasmoids: AndromedaLauncher, KdeControlStation, simplekickoff, modernclock, weatherWidget-3.
Unused themes: Gently, McMojave, ChromeOS desktopthemes; ChromeOS/Layan/Orchis aurorae; cachyos-emerald (SVG references only).

## Replace plan (inventory view)
| Component | Replace? |
|---|---|
| Plasma style, colors, LnF, GTK, panel colorizer, spacer, blur tuning | yes — own |
| Klassy style + decoration | yes — own C++ (user decision) |
| Glass effect | drop; own effect instead |
| Kvantum | drop |
| Icons, cursor, fonts | keep as deps |
| Kurve, apdatifier, Chaac weather | keep, optional |
