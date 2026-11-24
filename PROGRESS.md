# Progress (write all code first, then build/test)

## Write phase
- [x] tokens/tokens.toml
- [x] tools/lumen: Cargo, main, tokens, color, ctx, svg
- [x] gen: cpp header, qml tokens, colors, terminals, assemble
- [x] gen: desktoptheme (Plasma SVG; ids lumen-dark / lumen-light)
- [x] gen: gtk3/gtk4 + libadwaita, firefox, union
- [x] gen: boot (plymouth two-step, limine, vconsole), wallpaper, sounds
- [x] install / apply / doctor / uninstall / pacman hook
- [x] CMake top + common/
- [x] decoration/ (KDecoration3, titled + shadow-only)
- [x] style/ (QCommonStyle-based Fluent QStyle, animations, popup shadows+blur, window drag)
- [x] kwin/effects/materials (C++: Acrylic live + Mica cached wallpaper, tint uniform)
- [x] kwin/effects-js (lumen_open, lumen_minimize)
- [x] installer: cleanup (superseded packages), conflicting effects disabled on apply
- [x] tray: C++ containment fork + QML (manual order, overflow drag, Quick Settings tiles/sliders/media/battery, config page)
- [x] plasmoids: start (kicker models, pins, recommended, all apps, search, power), clock (notification center + calendar)
- [x] look-and-feel: defaults, layout migration script, splash, windowswitcher, previews
- [x] shell overlay: lock screen password block, logout tiles; pacman hook refresh
- [x] README
- [x] settings module `kcm_lumen` (System Settings → Appearance): lumenrc schema generated from tokens, 8 pages, live reload in style/decoration/effect
- [x] Makefile (build, bench, kcm, install[-system], uninstall[-system], apply[-layout], doctor, snapshot, clean)

## Build/test phase
- [x] cargo build (0 warnings), `lumen gen` (6.6 s cold / 0.12 s cached)
- [x] cmake build all C++ (0 warnings, 26 s clean at -j32): lumen6, org.lumen.decoration, lumen_materials, org.lumen.tray
- [x] QML lint (no syntax/API errors; remaining hits are dynamic-type false positives)
- [x] windowed runtime tests: clock flyout, Start menu, tray QML load; style in KWrite
- [x] style paint benchmark (`bench/stylebench`, 480x720 gallery): Lumen 0.48 ms min / 0.71 ms median vs Breeze 0.85 / 1.76, Klassy 0.79 / 1.64, Fusion 1.41 / 2.72 (9-slice cached panels)
- [x] `lumen snapshot` dry run against the real appletsrc
- [x] settings module: all pages render in kcmshell6 + System Settings (first item under Appearance), qmllint clean
- [x] optimization pass:
  - gen warm 108 → 14 ms (cached sounds, independent steps + per-mode Plasma style + wallpaper sizes in parallel); cold 4.7 s (4K wallpaper blur, cached by SVG hash)
  - style: hashed 9-slice cache key, ~6% less paint time (A/B); now ~2x faster than Breeze on the same machine/load
  - materials: Mica re-blurs only when the wallpaper itself is damaged (window moves over the desktop no longer trigger full-screen passes); recapture after chain reallocation (strength change)
  - decoration: title fill clipped to the repaint area
  - clock: DND state expires on the minute tick
- [x] `lumen install --system` + `lumen apply` on the real session (2026-11-02)
- [x] in-session fixes: L&F `defaults` placeholders (icons/cursor), per-app Klassy/Kvantum scheme pins dropped on apply,
  Plasma frame seams (fills + hint-stretch-borders), wallpaper never overridden, tray popup reopen loop (containment
  `expanded` always reads true), tray clicks (drag layer swallowed them; inline applets spilled over neighbours),
  applets adopted on creation, overflow flyout sized to content, Start app list populated on first open,
  KWin 6 minimize hook, settings module auto-applies (no Apply button)
- [x] taskbar: own task manager fork (adjustable 16–64 px icons independent of panel height), Breeze-sized fonts, user's tuned values as defaults, clock flyout height/holiday labels, exact-size icons everywhere
- [ ] after re-login: verify materials blur/Mica (new effect build) and rounded title-bar corners (new decoration)
- [ ] in-session checks: lock + logout overlay, materials effect FPS
