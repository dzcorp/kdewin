# Lumen settings audit — 2026-11-05

Audited all 58 settings exposed by the Lumen module, its 12 tool actions, widget configuration links, and related KDE modules. Found and repaired broken notifications, runtime readers, restoration, and reapply behavior.

## Coverage

| Page/group | Count | Settings covered |
|---|---:|---|
| Main / Appearance, Fonts | 2 | Mode; TextSize |
| Materials | 8 | Enabled; AcrylicStrength; AcrylicNoise; AcrylicSaturation; MicaStrength; MicaSaturation; MicaForPanels; WallpaperTone |
| Windows / Decoration | 7 | TranslucentTitlebar; TitlebarOpacity; TitlebarHeight; CenterTitle; ShowIcon; ButtonLayout; CornerRadius |
| Applications / Style | 8 | TranslucentWindows; WindowOpacity; OpaqueApps; ControlRadius; TranslucentMenus; ScrollbarExpanded; Animations; AnimationDuration |
| Applications / Files | 6 | CommandBar; OpenTo; ThisPcFreeSpace; ThisPcFolders; ThisPcNetwork; ThisPcHiddenDrives |
| Animations | 4 | WindowOpen; Minimize; OpenScale; WindowDuration |
| Taskbar layout/icons | 10 | height; floating; autoHide; centered; startIcon; appIcon; tasksSpacing; trayIcon; quickIcon; traySpacing |
| Taskbar clock | 9 | clockMode; clockDateStyle; clockHour; clockSeconds; clockBold; clockTimeSize; clockDateSize; clockTimeAlign; clockDateAlign |
| Lock screen | 3 | LumenWallpaper; AlwaysShowClock; MediaControls |
| Notifications | 1 | notifyOverFullscreen / inverted DoNotDisturb.WhenFullscreen |
| **Total** | **58** | |

The 38 schema properties were traced from QML controls to generated names/types/ranges, saving, and their consumers. The 19 taskbar values were checked against actual generated Plasma scripting and widget configuration keys. Notification handling uses Plasma's native setting.

## Repairs

- Generated settings now use `Notifiers=true`. The old `KConfigSkeletonItem*` loop called a nonvirtual setter on signalling wrappers rather than their wrapped items, so saving did not notify open applications.
- Automatic mode now leaves scheduling to KDE's installed autoswitcher. Manual applications are serialized, preserve the explicit automatic-mode flag, and finish the latest request even if the module closes. Failures are reported on the main page.
- Master Materials updates decoration geometry, QWidget backgrounds/menus, and Dolphin rendering and blur requests.
- Widget animation hints and indeterminate progress honor the animation switch and duration; disabling animations stops active timers. Custom QQC2 delegates and switches use a watched `org.lumen.settings` singleton for radius and motion settings.
- Existing menus can change translucency without recreating the application. Opaque-app names are trimmed around commas.
- Acrylic retains fractional blur offsets, and noise level 1 produces grain.
- Central clock mode clears the legacy `showDate` override. Regional short dates no longer silently use a custom format. Tray spacing text matches the runtime gap, and vertical task icons honor their configured size within available space.
- Notification Defaults and external changes update the module.
- Tool startup failures release busy state and report errors. Successful apply/restart refreshes taskbar detection and controls.
- Lock wallpaper opt-in selects the image plugin and backs up/restores the previous plugin, image, and preview. An externally selected plugin is preserved. Always-show-clock clears the conflicting idle-hide setting.
- Repeated boot activation preserves the first Limine backup until restoration.
- CLI reapply preserves disabled effects, durations, and scale; reconfigures enabled effects; persists mode; notifies the automatic-mode service; and uses the same lock-wallpaper backup contract.

## Verification

Run:

```sh
make check-settings
make bench-style ITER=100 STYLES=Lumen
tools/lumen/target/release/lumen doctor
```

`bench/settingscheck.cpp` loads the real KCM using KDE's loader, temporary XDG configuration/data/cache directories, a private D-Bus session, and mock desktop services. Privileged tool actions use inert test executables.

The checks cover:

- All schema properties, generated change notifications, persisted key existence and values, KWin mirroring, and font scaling.
- All 19 taskbar values through the generated script, persisted values, legacy clock state, Defaults, and detection after applying a missing panel.
- Two QML engines observing live radius and animation updates; widget animation hints, paused busy indicators, and existing-menu alpha changes.
- Notification inversion, external updates, and Defaults.
- Lock wallpaper activation/restoration, including a different original plugin and an external plugin change; idle clock behavior.
- All nine real QML pages loading, all 12 tool actions dispatching, and tool startup failure recovery.
- Rapid manual/automatic changes and closing the module during a pending mode application.
- The real CLI reapply using temporary settings and inert desktop commands, preserving disabled effects and custom animation values and restoring the original lock plugin.

Settings-save timing in the isolated test was approximately 0.45–0.55 ms, excluding the intentional 250 ms debounce.
The offscreen Lumen widget paint benchmark measured 500 µs median and 648 µs p95 over 100 frames at 480×720.
The rebuilt components were reinstalled, the desktop shell reloaded successfully, and `lumen doctor` passed. The saved `lumenrc` checksum was unchanged.

## Practical limits

The test services verify configuration and dispatch without altering the desktop. Runtime consumers were also inspected; this does not mean every possible application/window combination was visually exercised. File Explorer startup destinations apply to new windows; changing which application windows support translucency may require opening a new window, as described in the UI.

Boot/login actions were traced and their dispatch checked without rebooting, logging out, or running the privileged actions. Their final appearance still requires a boot/login check. Restart System Settings and reopen applications to load rebuilt native plugins; subsequent supported setting changes update live.
