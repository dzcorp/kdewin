# KDE theming API — Plasma 6.8.2 / KF 6.31 / Qt 6.12 (2026-11-02)

Tags: [L] local · [W] primary source · [2] secondhand · [I] inference.
Releases: 6.5 2025-10-21 · 6.6 2026-02-17 · 6.7 2026-06-16 · 6.8 2026-10-14 · 6.8.2 2026-10-27.

## 1. Union (new style engine)
- 6.7 alpha (QtQuick), 6.8 beta (+QtWidgets); not default. Enable: Application Style → "Breeze (Union)".
- Installed [L] `union 6.8.2`: `plugins/styles/UnionWidgetsStyle.so`, `plugins/union/{union-input-css,union-platform-plasma,union-colorprovider-kcolorscheme}.so`, Kirigami platform `org.kde.union`, headers `/usr/include/union/Union/`, tools `union-styletool`, `union-ruleinspector`. Styles `/usr/share/union/styles/{breeze,…}`.
- Arch: InputPlugin (CSS) → Style/StyleRule/Selector → OutputPlugins (QtQuick, QStyle w/ QCommonStyle fallback). ColorProvider reads KColorScheme.
- Package: `<id>/metadata.json` (`input-type:"css"`, `input-version:1`) + `contents/css/style.css` + `contents/images/`. Breeze = 16 CSS files ~3100 lines; `:root` vars, `custom-color("kcolorscheme",…)`, `mix()`, `modify-color()`, `&` nesting.
- Selectors: type/id/class(hints)/attribute; `:hovered :pressed :checked :disabled :highlighted :active-focus :visual-focus`; descendant/child.
- Props: background-color/image, border, border-radius, box-shadow, outline, font, icon-*, padding, spacing, opacity.
- **Limits:** no transitions/animations, no calc/@media, px only, no gradients/backdrop blur documented; widgets "some aspects off"; does NOT cover Plasma shell.
- Tool: `union-styletool create|install|package`; `UNION_STYLE_NAME=<id>` per app.
- Docs: https://api.kde.org/union-index.html · https://api.kde.org/css-support.html · https://api.kde.org/css-properties.html

## 2. QtWidgets QStyle
- C++ QStyle still production path. Fork Breeze at tag **v6.8.2** (master = 6.9 dev, Qt≥6.11, KF≥6.30).
- Breeze `kstyle/`: breezestyle.cpp, breezehelper, breezetileset, breezeshadowhelper, breezeblurhelper, breezewindowmanager, breezetoolsareamanager, animations/, config/.
- Deps: Qt6 Core/Widgets/Svg/DBus/Quick; KF6 CoreAddons/ColorScheme/Config/GuiAddons/I18n/IconThemes/WindowSystem/KCMUtils/FrameworkIntegration; KDecoration3; ECM.
- Register: plugin json key + `/usr/share/kstyle/themes/<x>.themerc` (`[KDE] WidgetStyle=`, `ConfigPage=`).
- Qt 6.12 ships `QtQuick/Controls/FluentWinUI3` — reference only.

## 3. QtQuick / Kirigami
- `org.kde.desktop` (qqc2-desktop-style) paints via app QStyle → our QStyle restyles Kirigami apps too.
- `org.kde.breeze` pure QML (mobile). Colors from KColorScheme via plasma-integration.

## 4. KDecoration3 [L `/usr/include/KDecoration3`]
- `Decoration::paint(QPainter*, QRectF)`, `init()`; setters setBorders, setResizeOnlyBorders, setBlurRegion, setTitleBar, setOpaque, setShadow, **setBorderRadius**, **setBorderOutline**; `popup(Positioner, QMenu*)`.
- `DecorationState` double-buffered; `BorderRadius` per-corner; `BorderOutline` thickness/color/radius. KWin clips corners compositor-side (6.5+).
- 6.3 break: float geometry, client→window. Fractional: `window()->nextScale()`, `pixelSize(scale)`, `snapToPixelGrid()`.
- **6.8:** shadow-only decorations (`"styles":["titled","shadow"]`) → KWin gives CSD windows (Steam, Discord, Firefox CSD) our shadow + outline + rounded corners.
- Translucent titlebar: paint alpha + `setOpaque(false)` + `setBlurRegion`.
- KWin 6.8 blur protocol: `ext_background_effect_manager_v1` (org_kde_kwin_blur dropped in 6.7 → old clients lose blur).
- Aurorae V1/V2 present; slower, fractional scale issues → prototype only.

## 5. Plasma style (KSvg SVG)
- `/usr/share/plasma/desktoptheme/<id>/`; metadata.json `{"KPlugin":{Id,Name,…,Version},"X-Plasma-API":"5.0","KPackageStructure":"Plasma/Theme"}`. Id = dir name; bump Version to bust cache. Partial themes fall back (`[Settings] FallbackTheme`).
- Default theme: `dialogs/background`, `widgets/` 40+ (panel-background, background, tooltip, button, tasks, plasmoidheading, scrollbar, lineedit, slider, switch…), `{translucent,opaque,solid}/` variants, plasmarc `[AdaptiveTransparency]`. `icons/` deprecated.
- Frames: edges/corners/center; prefixes north-/south-/east-/west-/thick-; `shadow-*`; **`mask-*` = blur/contrast region**.
- Hints: `hint-{top,bottom,left,right}-margin`, `-inset`, `shadow-hint-*`, `hint-tile-center`, `hint-stretch-borders`, `hint-no-border-padding`, `hint-compose-over-border`, `hint-apply-color-scheme`, `hint-bar-size`, `hint-handle-size`, `hint-scrollbar-size`, `hint-focus-over-base`.
- plasmarc `[BlurBehindEffect]`, `[ContrastEffect]` (enabled, contrast, intensity, saturation).
- Colors: `<style id="current-color-scheme">` + `ColorScheme-{Text,Background,Highlight,HighlightedText,PositiveText,NeutralText,NegativeText}` and Button/View/Tooltip/Complementary/Header variants; `fill="currentColor"`. Accent = `ColorScheme-Highlight`.

## 6. Color scheme (.colors)
- Sections: `[ColorEffects:*]`, `[Colors:Button|Complementary|Header|Selection|Tooltip|View|Window]`, `[Colors:Header][Inactive]`, `[General]`, `[KDE] contrast`, `[WM]`.
- kdeglobals `[KDE] frameContrast` (KF 6.20+). Accent: kdeglobals `[General] AccentColor`, `accentColorFromWallpaper`; applyScheme overwrites DecorationFocus/Hover, ForegroundActive/Link, Selection — **don't hardcode accent**.
- Scheme `[General] TintFactor` (OKlab tint), `TitlebarIsAccentColored`.

## 7. Look-and-feel package
- metadata.json `KPackageStructure:"Plasma/LookAndFeel"`, `X-Plasma-APIVersion:"2"`. No symlinks.
- `contents/defaults` `[rcfile][Group] key=value` (kdeglobals widgetStyle/ColorScheme/Icons, plasmarc Theme, kcminputrc cursorTheme, kwinrc decoration, ksplashrc).
- `contents/layouts/org.kde.plasma.desktop-layout.js`, `splash/Splash.qml`, `previews/`, `plasmoidsetupscripts/`, logout, windowswitcher, lockscreen (via shell fallback `/usr/share/plasma/shells/org.kde.plasma.desktop/contents/{lockscreen,logout}`).
- Day/night: `lookandfeelautoswitcher` kded (6.5+); light/dark/twilight variants (6.7).

## 8. Icons / cursors
- Icons: SVG `current-color-scheme` classes incl. `ColorScheme-Accent`; `-symbolic` 16px. (We keep existing icons.)
- Cursors: `cursors_scalable/<name>/` SVG + `metadata.json` `[{filename,delay,hotspot_x,hotspot_y,nominal_size}]`; `kcursorgen --svg-theme-to-xcursor`. Missing metadata.json → invisible on Wayland.

## 9. Per version
- 6.5: Breeze rounded bottom corners; auto light/dark global theme; wallpaper light/dark.
- 6.6: custom global theme snapshots; frame intensity; Plasma Login Manager.
- 6.7: Union alpha; light/dark toggle widget; twilight; rounded list highlights; blur protocol → ext-background-effect-v1.
- 6.8: Wayland-only; Union beta widgets; shadow-only decorations for CSD; no radius on tiled windows; accent colorization Oxygen/Air.

## 10. Login + GTK
- **This machine uses Plasma Login Manager** (`plasmalogin`), no SDDM. Greeter QML compiled in → only wallpaper/settings theming (`/etc/plasmalogin.conf`).
- breeze-gtk: Sass + PyCairo build → gtk-2/3/4. kde-gtk-config writes `~/.config/gtk-{3,4}.0/{settings.ini,colors.css,gtk.css}`.
- libadwaita: ignores theme name; only `~/.config/gtk-4.0/gtk.css` named colors (`window_bg_color`, `accent_bg_color`…). Portal exposes color-scheme + accent-color.

## Recommended engine per component
| Component | Engine | Why |
|---|---|---|
| QtWidgets | C++ QStyle (fork Breeze v6.8.2) | mature, animated, also drives Kirigami |
| QML/Kirigami | QStyle via org.kde.desktop (+ optional Union CSS later) | Union beta, no animations |
| Decoration | C++ KDecoration3 (QPainter) | radius/outline/shadow/blur hooks, fractional scale |
| Plasma shell | KSvg SVG theme | only supported shell engine |
| Colors | .colors w/ Header, no hardcoded accent | KDE applies accent/tint |
| Global theme | LnF package | bundles all |
| Login | PLM settings | no QML theming |
| GTK | own Sass/CSS gen + gtk-4.0 gtk.css overrides | libadwaita only via named colors |
