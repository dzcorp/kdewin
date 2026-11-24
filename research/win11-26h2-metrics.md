# Windows 11 26H2 (build 26300) measurements

Measured 2026-11-03 in a VM at 1280×800, 100 % scale, light theme (dark only for File Explorer: personalization
is locked on an unactivated copy). Logical px. Colours are opaque fallbacks (no GPU, so no Mica/Acrylic).

## Type
- Win32 text (Explorer items, nav pane, tabs, command bar, status bar, title bars, tooltips): Segoe UI 9 pt = 12 px.
- XAML: Caption 12 px (Start tile labels, clock, QS tile labels, toast app name), Body 14 px (menus, search
  placeholder, notification title/body, calendar days, address bar), BodyStrong 14 px (Start section titles).

## Taskbar
- Height 48, 1 px top stroke. Buttons on a 44 px pitch, plate 40×40 (2 px inset), radius 4.
- Hover/active plate: light fill over the bar (~white 50 %) with a 1 px elevation stroke (bottom darker).
- App icons 24. Start glyph 24. Running indicator 3 px tall at the plate bottom: active 16 px accent, inactive 6 px.
- Tray: icons 16; overflow chevron and single icons take 32×40 plates; network+volume(+battery) is one plate
  with 16 px icons on a 28 px pitch and 8 px side padding (2 icons → 60 px).
- Clock: two right-aligned lines, Caption 12 px each, 16 px apart, 6 px inner padding; plate 40 tall.
- Show-desktop strip 12 px at the right edge.
- Popups stop 12 px above the bar; Notification Center, QS 12 px from the right edge; toasts 16 px.

## Start (single scrolling page)
- 640 wide (incl. 1 px border), radius 8, height 691 on an 800 px screen.
- Header band 64 px (search pill 32 tall, radius 16, 32 px side margins), 1 px divider.
- Content: 32 px side margins; section titles BodyStrong; "Show all ⌄" / "View: Category ⌄" are borderless.
- Pinned grid: 6 columns × 96, tiles 96×84, icon 32 (12 px from tile top), label Caption, 2 rows collapsed.
- Sections: Pinned → Recent (Recommended) → All, All view = Category (default) / Grid / List.
- Category cards 156×156 (radius 8, 2×2 icons 32), 22 px gaps, label under the card.
- List rows 40 tall, icon 24; letter headers.
- Footer band 64 px (darker), 1 px divider, avatar 32 + name Caption, power button.
- Menus (view picker, power): radius 8, 4 px padding, items 32 pitch.

## Quick Settings
- 360 wide, 24 px padding, tiles 96×48 (radius 4), 12 px column gap, 96 px row pitch (tile + label).
- Divider, volume slider row (track 4, thumb 20), footer band 48 with settings gear.

## Notification Center + calendar
- Two panels, 336 wide, 12 px apart, radius 8.
- Notifications panel: header "Notifications" Body + DND button + "Clear all" button; per-app header (icon 16 +
  name Caption) above each app's notifications; each notification is a card (radius 4, 1 px stroke, 4 px inset
  from the panel edge) with time Caption, image 48, title Body, body Body secondary.
- Calendar panel: header row 48 (long date, collapse chevron), month title BodyStrong 14→ 18 px,
  up/down arrows, 7 columns on a 47 px pitch, rows 42, today circle 28; footer 48 (focus timer).

## Toast
- 364 wide (incl. border), radius 8; 16 px padding; header row: app icon 16 + app name Caption, "…" and ✕ at right;
  title Body (regular), body Body secondary, 20 px lines.

## Menus (XAML context menu)
- Radius 8, item 28 tall + 2 px vertical margin = 32 pitch, 4 px side inset, icon 16, text Body 14 px,
  shortcut Caption 12 px secondary, separators 1 px.

## File Explorer
- Tab strip 40 (selected tab 30 tall, joins the address row), address row 48 with a 32 px box, divider,
  command bar 46 + divider, nav pane rows 32, details rows 28, column header ~24, status bar ~24.
- Light: tab strip #E8E8E8, address row #F8F8F8, box #FDFDFD, content #FFFFFF, dividers #D6D6D6;
  row hover #E5F3FF, selected #CCE8FF + 1 px focus rect.
- Dark: tab strip #202020, address row #2C2C2C, box #383838, content #191919, dividers #3A3A3A;
  row hover #4D4D4D, selected #505050; nav selection #333333.
- 2560×1440 details view (2026-11-04): caption buttons 46×30 from the window edge; address box → search gap 8,
  search 11 px from the edge; command bar 46 + divider #D6D6D6 / #3A3A3A.
- Column header 25 tall, no rule below, full-height dividers after each column (#E5E5E5 / #636363), text 7 px in,
  vertically centred (#4C607A / #DEDEDE), hover fill #D9EBF9 / #434343, sort chevron 9×5 hairline at the top centre
  (#595959 / #ABABAB, up = ascending). Name header starts 25 px into the view; rows' band starts 15 px in.
- Rows 31 px pitch, highlight 27 (2 px clear above and below), square: hover #E5F3FF / #4D4D4D, selected #CCE8FF /
  #505050, selected without focus #D9D9D9 (guess) / #333333; icon 4 px into the band, name 5 px after the icon;
  other columns' text 6 px in, #6D6D6D / #DEDEDE (selected light rows darken it); band ends 4 px before the last
  column's end; columns do not stretch.
- Win32 text is Segoe UI 12 px with TrueType hinting: caps 9 px (FreeType hintslight gives 8 → `hintfull` for
  Segoe UI/Consolas via fontconfig). XAML text (clock) already matched with Segoe UI Variable.
- Navigation tree: rows 32, chevron centre 16 px from the pane edge, icon at 34, +8 px per level.

## Native resolution findings (2026-11-03, VM switched to QXL WDDM driver)
VM setup: libvirt `win11`, QXL with `<resolution x='2560' y='1440'/>`, vgamem 32 MB (XML backup:
scratchpad/win/win11.xml.bak). Windows had only "Microsoft Basic Display Adapter" (resolution fixed by OVMF
NVRAM, `shutdown /r /fw` unsupported, Esc-at-boot unreliable) → installed `qxldod` from
/var/lib/libvirt/images/virtio-win.iso (`pnputil /add-driver D:\qxldod\w10\amd64\qxldod.inf /install`), then
Settings › Display › resolution works (list up to 3840×2160). The "Keep changes" dialog reverts after 15 s.
Login needs the user's PIN after a VM reboot. Helper scratchpad/win/vm.py reads the size from the last shot.

### Start menu adapts to the screen (main native-res difference)
| screen | mode | menu (incl. border) | pinned cols | category cols |
|---|---|---|---|---|
| 1280×800 | small | 640 × 691 (height cut to screen) | 6 + "Show all" | 3 |
| 1366×768 | small | 640 × screen-limited | 6 | 3 |
| 1280×1024 | small | 640 × 726 | 6 | 3 |
| 1440×900 | small | 640 × 726 | 6 | 3 |
| 1600×900 | large | 834 × 791 (cut: 852 − 61) | 8 | 4 |
| 1920×1080 | large | 834 × 865 | 8 | 4 |
| 2560×1440 | large | 834 × 865 | 8 (all 16 pins fit, no "Show all") | 4 |
- Rule: large when screen width ≥ 1600 (1440 small, 1600 large); height = min(cap, available − 61) with cap
  726 small / 865 large; 12–13 px above the taskbar.
- Start is CENTRED ON THE SCREEN (2560: x 863…1696, centre 1279.5), not on the Start button.
- Large: 8 × 96 = 768 grid + 32 px margins; category cards 156, 4 per row.

### Other surfaces at 2560×1440
- Quick Settings: same 360 × (content) as at 1280×800.
- Notification Center: same 336 width (x 2212…2547, 12 px from the right edge); grows up to the screen top with
  more notifications; per-app header rows + cards (e.g. "Windows Security", "+1 notifications" chip, Dismiss button).
- Calendar panel unchanged (336 wide, 47×42 cells, today circle 40).
- Taskbar unchanged (48, 1 px top stroke #bebebe on #eeeeee light).

### Tray and clock at 2560×1440 (pixel-measured, light)
- App icons: 16 px boxes, full-bleed artwork (OneDrive 16×14, location 14×16), 32 px pitch; chevron centred in the
  first 32 px cell.
- Quick Settings glyphs: Ethernet 16×16 (monitor 13 wide with rounded 1 px corners, plug 5×7 top right, cable down
  the right, stand 2 legs + 9 px foot), Volume3 15×14 (speaker 3 px box + 45° cone, waves r 3/6/9 around (5.5, 8),
  ±50°); 28 px pitch (network box x 2424, volume 2452/2453). Strokes 1 px, black on light.
- Clock: Segoe UI Variable 12 px with proportional digits (date "03.11.2026" ink 55 px); time ink rows 1403–1411,
  date 1419–1427 (baseline 12 px into each 16 px line); plate 72×40 (x 2476–2547), then a 12 px show-desktop strip.

## Done in Lumen (2026-11-03, second pass)
- Quick Settings order follows quickSettingsItems (network, volume, battery); own 16 px glyphs (StatusGlyph.qml:
  wired, Wi-Fi levels, offline, mobile, airplane, volume 0–3/mute, battery, Bluetooth, microphone, USB, printer,
  clipboard, phone) replace the icon theme's padded 22 px art in the QS button and tray items.
- App tray icons fill the 16 px cell (SystemTray::iconPlacement, measured at the render size).
- Clock: no tabular digits, baseline −1 px, 12 px show-desktop strip (clicks toggle show desktop; only when the clock
  ends the panel).
- Calendar: KDE features kept in the Windows style: year and decade views from the title, wheel and keyboard
  navigation, week numbers, alternate-calendar labels, event colours, Today button when away.
- Start: wide mode (≥1600 px: 834×865, 8 columns, 4 category columns), small cap 726, height
  min(cap, available − 61); centred on the screen for a centred horizontal taskbar (CompactApplet popupAnchor patch).
- Panel at top/left/right checked: popups open beside the panel with the 12 px gap; vertical clock shrinks to fit.
- Windows fonts: `lumen fonts <Windows root>` copies Segoe UI, Segoe UI Variable, Segoe Fluent Icons, MDL2, Symbol and
  Consolas into fonts/windows (git-ignored, Microsoft licence) and installs them; `install` reinstalls them. `apply`
  uses Segoe UI for title bars and Dolphin item views, Consolas for fixed. The VM disk was read via
  `qemu-nbd --read-only --image-opts …file.locking=off` + kernel ntfs3 (ntfs-3g cannot read WOF-compressed files).

## Open items (to do next)
1. Collect everything else measurable: WindowMetrics registry, NONCLIENTMETRICS fonts, caption buttons, title bar
   height, window corner radius/shadow, tooltips, Alt+Tab, desktop icon size/spacing, volume OSD, context menu at
   native res, Explorer at native res vs Dolphin at the same window size.
2. Toast popups: still stock Plasma (bold title, 12 px, no app icon, ~347 wide) — own popups offered.
3. Taskbar app icons: some theme icons have 1–2 px padding (ink 21–22 vs Windows 23–24); could reuse the tray's
   artwork fit with a small clamp.
4. Optional: with Segoe Fluent Icons imported, StatusGlyph could draw the real Windows glyphs.

## Notification Center and toasts at 2560×1440 (2026-11-04, light)
- Two windows: NC panel x 2212…2547 (336), calendar panel below, 12 px apart, 12 px above the taskbar. With any
  notification the NC panel always reaches y = 8 (full height, empty space below the list); empty: 232 tall with
  only the header and a centred "No new notifications" (no icon, no Clear all).
- Header: "Notifications" Body at 18 px from the border; 24 px buttons (DND square, "Clear all") 16 px from the right.
- App header row: icon 16 + name Caption; "…" and ✕ appear on hover. Cards 4 px inside the panel, fill #F2F2F2,
  stroke #E0E0E0, 12 px padding; time of day Caption ("20:07", yesterday's too); title Body; body secondary;
  "Dismiss" 32 tall full width; "+N notifications" chip = standard button 24 tall (rest #FBFBFB, hover #F6F6F6).
  Card hover shows "…" and ✕ in its time row.
- Opening the NC removes the toasts on screen (they stay listed).
- Toast: 364×109 (one text line), 16 px from the right, 13 px above the taskbar; one at a time (others queue);
  slides in from the right (~7 px left to go after 0.4 s), stays 5 s, slides right + fades out.
- Start "View: Grid ⌄" / "Show all ›": borderless; hover = ControlFillColorDefault plate (#FBFBFB on #F2F2F2),
  32 tall, radius 4, 12 px left / 11 px right padding, chevron secondary.

### Done in Lumen (2026-11-04)
- NC is its own Plasma dialog above the calendar popup (Flyout.qml centerPanel + NotificationPanel.qml): full height
  with notifications, 232 px empty panel; it never takes focus, and the calendar popup stays open while the pointer
  is over it (root.centerHovered). Times are clock times (date after a day). Group ✕ on hover only; the group
  expand control is a standard button like Windows' chip. Opening the NC dismisses live toasts (kept, with actions).
- Start header buttons share LinkButton.qml (Windows hover plate).
- Toasts: own fork org.lumen.notifier (Windows geometry and layout, max three, equal action buttons, inline reply
  field); Windows queues one at a time for one app, Lumen stacks up to three.
- Still open: "+N" chip inside the newest card with the time-row chevron.

## File Explorer This PC at 2560×1440 (dark, 26H2)

- Content from the pane divider / command bar line: group header "Devices and drives" cap centre 18 px down, chevron
  centre x 11, title x 22, title ≈1.25× the item font; no Folders group in 26H2 (Win10 had one); "Network locations"
  only for added network places.
- Tiles 250×52 at x 14, y 34, pitch 261: icon 48 at +2/+2, text x +56; name cap top +5; capacity bar 189×13 at
  +56/+19 (border #BCBCBC, track #E6E6E6 in both themes, used accent, red when <10 % free); "18,5 GB free of
  48,9 GB" cap top +36, secondary. A drive without a bar (CD, unmounted) keeps the name at the first-line spot.
- Navigation pane: divider line (inset 6/5) 17 px under the last pin, "This PC" row 16 px under it, drives one
  level (8 px) in. Up from a drive root goes to This PC; File Explorer opens to Home or This PC (Folder Options).

### Done in Lumen (2026-11-04)
- thispc:/ KIO worker + Dolphin page with these metrics; navigation tree root "This PC"; Up from a drive root.
- lumenrc [Files] OpenTo (This PC default / previous session / recent files / home), ThisPcFreeSpace, ThisPcFolders,
  ThisPcNetwork, ThisPcHiddenDrives — Lumen settings → Applications and Dolphin's own startup page.
- Dolphin is "File Explorer" in every language (launcher + window titles); icon stays KDE's.
