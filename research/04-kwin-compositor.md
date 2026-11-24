# KWin compositor-side theming — KWin 6.8.2 Wayland, NVIDIA 615.78 (GLES/EGL) (2026-11-02)

## Plan-changing facts
1. Installed glass.so built for 6.6.4 → not loaded. If any blur fork is used, stock `blur` + `translucency` must be off.
2. **KWin ≥6.5 has native rounded corners + outlines**: SDF clip in core renderer driven by `Window::borderRadius()` from decoration (`setBorderRadius`, `setBorderOutline`). Single pass, no offscreen → ~free.
3. `org_kde_kwin_blur` / contrast removed in 6.7. Only `ext_background_effect_manager_v1` (region only) + `org_kde_kwin_shadow` v2. → clients can't pick strength/tint; strength global; tint = surface alpha.
4. Effect ABI = per patch release (IID embeds `KWIN_PLUGIN_VERSION_STRING "6.8.2"`) → rebuild C++ effects on every KWin update (pacman hook).

## Effect API
- C++: `KWin::Effect` (`prePaintWindow`, `paintWindow`, `drawWindow`), API 0.237. `OffscreenEffect` = per-window swapchain + fence (costly). Shaders via `ShaderManager`, **GLSL ES only** (6.8 GLES-only).
- JS: KPackage `KWin/Effect`, `~/.local/share/kwin/effects`, no rebuild. `animate/set/retarget/cancel`; attrs Opacity, Scale, Translation, Size, Clip, CrossFadePrevious, Shader. Curves = `QEasingCurve::Type` only (no bezier). Cannot blur/sample wallpaper/set radius.
- QML `QuickSceneEffect`: fullscreen overlays (overview etc.).

## Rounded corners
- SSD: decoration `setBorderRadius(8)` → native.
- CSD (GTK/Electron/Firefox): (a) 6.8 rule "Titlebar: Only shadow" (`DecorationPolicy::Shadow`) needs decoration json `"styles":["titled","shadow"]` → compositor adds shadow+outline+radius; (b) tiny C++ effect calling `Window::setBorderRadius` (Glass trick `blur.cpp:1503`).
- ShapeCorners = OffscreenEffect per window → avoid.

## Blur / Mica
- Stock blur: dual-Kawase, `BlurStrength` 1-15, `NoiseStrength`, `Saturation`; rounded when `borderRadius()` set; region from surface / decoration `blurRegion` / Plasma SVG `mask-*`. Skips transformed windows unless `WindowForceBlurRole`.
- Acrylic ≈ stock blur tuned + tint via SVG/scheme alpha.
- **Mica**: not in KWin. forceblur "static blur" path (`createStaticBlurTextureWayland`, cached `m_staticBlurTextures`) — render+blur wallpaper once per output, sample at screen UVs → port into own effect. Low runtime cost.

## Shadows
- SSD: `DecorationShadow` 9-patch QImage (bake 2 Fluent layers).
- Plasma dialogs/panels: SVG `shadow-*` + `shadow-hint-*-margin` → `org_kde_kwin_shadow`.
- CSD: "Only shadow" rule + our decoration's shadow style.

## Animations
- Built-in: `scale` (InScale/OutScale 0.8, 200ms), `squash` (minimize 250ms), `maximize` (250ms). Categories exclusive: `toplevel-open-close-animation`, `minimize`, `maximize`.
- Own JS effect: OutQuint/OutExpo ≈ Fluent decel; set `WindowForceBlurRole` during anim. Exact bezier → C++ `AnimationEffect`.

## Decoration
- KDecoration3 C++: `setBorderRadius`, `setBorderOutline`, `setBlurRegion`, `setShadow`, `palette()` (match app color), `popup(Positioner, QMenu*)`, `requestToggleMaximization`. Aurorae lacks radius/blur → no.
- Klassy `Defenestrated_11` preset = closest existing Win11 buttons (reference).
- Snap Layouts flyout: no upstream API. Needs C++ effect (`Window::requestTile`, `setQuickTileMode`, `pointerMotion`) + QML overlay. High complexity → later phase.

## NVIDIA
GLES, triple buffering default, IN_FENCE_FD OK for 615.x. Heavy blur → cursor stutter (try `KWIN_DRM_NO_AMS=1`). Avoid OffscreenEffect per window. Static Mica cheap.

## Rust
No KWin/KDecoration bindings. Rust only for tooling.

## Reference sources (session scratchpad clones)
kwin Plasma/6.8 (`windowitem.cpp`, `window.cpp`, `itemrenderer_opengl.cpp`, blur), klassy, glass, shapecorners, forceblur, aurorae, libplasma.
URLs: https://github.com/4v3ngR/kwin-effects-glass · https://github.com/paulmcauley/klassy · https://github.com/matinlotfali/KDE-Rounded-Corners · https://github.com/xarblu/kwin-effects-better-blur-dx · https://wayland.app/protocols/ext-background-effect-v1 · https://develop.kde.org/docs/plasma/kwin/api/

## Per-feature decision
| Feature | Approach | Complexity | Perf |
|---|---|---|---|
| Corners SSD | decoration `setBorderRadius(8)`, 0 when maximized/tiled | low | ~0 |
| Corners/shadow CSD | decoration `shadow` style + "Only shadow" rule; fallback radius-only effect | low-med | ~0 |
| Outline | `setBorderOutline` | low | ~0 |
| Shadows | baked 2-layer 9-patch; Plasma SVG shadow-* | low | ~0 |
| Acrylic | stock blur tuned (strength ~6-8, sat ~125) + alpha tint | low | med |
| Mica | own C++ effect: static wallpaper blur cache | high | low |
| Animations | own JS effect (OutQuint) → C++ later if needed | low-med | low |
| Snap Layouts | C++ effect + QML overlay | high | low |
