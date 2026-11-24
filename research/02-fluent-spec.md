# Fluent (Win11/12) design tokens + existing KDE Win11 themes (2026-11-02)

Tags: **[src]** MIT WinUI source / MS docs source · **[fluent2]** microsoft/fluentui tokens · **[web]** secondary · **unverified**.
XAML colors are `#AARRGGBB` (e.g. `#0FFFFFFF` = rgba(255,255,255,0.059)). Key file: WinUI `Common_themeresources_any.xaml`.

## A1. Geometry [src]
| Token | Value |
|---|---|
| ControlCornerRadius | 4px — buttons, checkbox, combo, textbox, list items, bars, ToolTip |
| OverlayCornerRadius | 8px — windows, dialogs, flyouts, menus, teaching tips |
| Square | 0px when maximized/snapped; touching edges (split button halves) |
| Stroke | 1px all surfaces |
| Selection pill | ListViewItem 3x16 r1.5; NavigationView 3x16 r2 |

## A2. Colors [src] (dark / light)
| Token | Dark | Light |
|---|---|---|
| TextFillPrimary | #FFFFFF | #E4000000 |
| TextFillSecondary | #C5FFFFFF | #9E000000 |
| TextFillTertiary | #87FFFFFF | #72000000 |
| TextFillDisabled | #5DFFFFFF | #5C000000 |
| TextFillInverse | #E4000000 | #FFFFFF |
| TextOnAccent Pri/Sec/Dis | #000000 / #80000000 / #87FFFFFF | #FFFFFF / #B3FFFFFF / #FFFFFF |
| ControlFill Default | #0FFFFFFF | #B3FFFFFF |
| ControlFill Secondary (hover) | #15FFFFFF | #80F9F9F9 |
| ControlFill Tertiary (pressed) | #08FFFFFF | #4DF9F9F9 |
| ControlFill Disabled | #0BFFFFFF | #4DF9F9F9 |
| ControlFill InputActive | #B31E1E1E | #FFFFFF |
| ControlStrongFill | #8BFFFFFF | #72000000 |
| ControlSolidFill | #454545 | #FFFFFF |
| SubtleFill Sec/Ter | #0FFFFFFF / #0AFFFFFF | #09000000 / #06000000 |
| ControlAltFill Sec/Ter/Quat | #19000000 / #0BFFFFFF / #12FFFFFF | #06000000 / #0F000000 / #18000000 |
| ControlStroke Def/Sec | #12FFFFFF / #18FFFFFF | #0F000000 / #29000000 |
| ControlStrokeOnAccent Def/Sec | #14FFFFFF / #23000000 | #14FFFFFF / #66000000 |
| ControlStrongStroke | #8BFFFFFF | #72000000 |
| CardStroke Default | #19000000 | #0F000000 |
| SurfaceStroke Default | #66757575 | #66757575 |
| SurfaceStroke Flyout | #33000000 | #0F000000 |
| DividerStroke | #15FFFFFF | #0F000000 |
| FocusStroke Outer/Inner | #FFFFFF / #B3000000 | #E4000000 / #B3FFFFFF |
| CardBackground Def/Sec/Ter | #0DFFFFFF / #08FFFFFF / #12FFFFFF | #B3FFFFFF / #80F6F6F6 / #FFFFFF |
| LayerFill Default / Alt | #4C3A3A3A / #0DFFFFFF | #80FFFFFF / #FFFFFF |
| LayerOnAcrylic | #09FFFFFF | #40FFFFFF |
| LayerOnMicaBaseAlt Def/Sec | #733A3A3A / #0FFFFFFF | #B3FFFFFF / #0A000000 |
| SmokeFill | #4D000000 | #4D000000 |
| SolidBackground Base/Sec/Ter/Quat/Quin/Sen | #202020 / #1C1C1C / #282828 / #2C2C2C / #333333 / #373737 | #F3F3F3 / #EEEEEE / #F9F9F9 / #FFFFFF / #FDFDFD / #FFFFFF |
| SolidBackground BaseAlt | #0A0A0A | #DADADA |
| Success / Caution / Critical | #6CCB5F / #FCE100 / #FF99A4 | #0F7B0F / #9D5D00 / #C42B1C |
| bg Success / Caution / Critical | #393D1B / #433519 / #442726 | #DFF6DD / #FFF4CE / #FDE7E9 |
| AccentFill Disabled | #28FFFFFF | #37000000 |

Accent shades (default Win11 blue) [web]: Light3 #99EBFF, Light2 #4CC2FF, Light1 #0091F8, Base #0078D4, Dark1 #0067C0, Dark2 #003E92, Dark3 #001A68.
- AccentFill: dark=Light2, light=Dark1; hover = same @0.9 opacity, pressed @0.8.
- Accent text: dark Light3/Light3/Light2; light Dark2/Dark3/Dark1.

Control elevation border: vertical gradient over first 3px, stops 0.33=ControlStrokeSecondary, 1.0=ControlStrokeDefault. Dark: top brighter. Light: flipped (bottom 1px darker). Accent buttons: OnAccent Sec/Def, flipped both themes.

## A3. Materials [src]
| Acrylic | Tint | TintOpacity | LuminosityOpacity | Fallback |
|---|---|---|---|---|
| Default dark | #2C2C2C | 0.15 | 0.96 | #2C2C2C |
| Base dark | #202020 | 0.5 | 0.96 | #1C1C1C |
| Default light | #FCFCFC | 0.0 | 0.85 | #F9F9F9 |
| Base light | #F3F3F3 | 0.0 | 0.9 | #EEEEEE |
| Accent | AccentDark1 (Base: Dark2) | 0.8 | 0.8 | accent |

Recipe: blur radius 30, noise 0.02, exclusion #1AFFFFFF, saturation 1.25, fade-to-fallback 500ms. Luminosity blend: backdrop gives hue/sat, solid layer gives lightness.
Mica: recipe unpublished; samples wallpaper once (no live blur). Fallback Mica=#202020/#F3F3F3, Mica Alt=#0A0A0A/#DADADA. Inactive window → fallback.
Layering: Mica base → LayerFillDefault content → (Mica Alt) LayerOnMicaBaseAlt. Acrylic only transient surfaces; never stacked.
KWin stock blur: global only (BlurStrength, NoiseStrength, Saturation) → per-surface Acrylic needs own effect.

## A4. Elevation / shadows [src + fluent2]
| Surface | Elevation | Shadow |
|---|---|---|
| Window, Dialog | 128 | `0 0 8px A, 0 32px 64px K` |
| Flyout / menu | 32 | `0 0 8px A, 0 14px 28px K` (or shadow16) |
| Tooltip | 16 | `0 0 2px A, 0 8px 16px K` (or shadow8) |
| Card | 8 | `0 0 2px A, 0 4px 8px K` |
| Control | 2 | `0 0 2px A, 0 1px 2px K` |
Alpha: light A .12 K .14; dark A .24 K .28 (darker: .20/.24, .40/.48). DWM active/inactive values unverified.

## A5. Typography [src]
Caption 12/16 400 · Body 14/20 400 · BodyStrong 14/20 600 · BodyLarge 18/24 · Subtitle 20/28 600 · Title 28/36 600 · TitleLarge 40/52 600 · Display 68/92 600. Control text 14, tooltip 12. Sentence case.
Segoe UI Variable proprietary — reference by name only, never bundle; fallback Selawik (OFL) / Inter.

## A6. Sizing [src]
| Item | Value |
|---|---|
| TextBox | min h 32, border 1, focus bottom 2px accent |
| Button | padding 11,5,11,6, r4, ≈32 h |
| CheckBox | 20x20 glyph box |
| ListViewItem | min h 40 |
| NavigationView | compact 48, item min h 36, icon 16 |
| Menu | presenter pad 0,2; item margin 4,2; item pad 11,4,11,5 (mouse); icon slot 28; separator 1px pad -4,1; min h 32 |
| Flyout | pad 16,15,16,17 |
| Dialog | w 320–548, pad 24, button gap 8 |
| ToolTip | pad 9,6,9,8, max w 320 |
| Title bar | 32 (48 with search); icon 16 @16px from edge; caption-style title |
| Caption buttons | 46x32 (unverified); close hover #C42B1C (unverified) |
| Taskbar | 48 (unverified) |

## A7. Motion [src]
Durations 83 / 167 / 250 / 333 ms.
| Curve | Bezier |
|---|---|
| Fast-in / entrance, FastOutSlowIn | (0,0,0,1) |
| Point-to-point | (0.55,0.55,0,1) |
| Fast-out exit | (0,0,0,1) 167ms + fade |
| Soft-out | (1,0,1,1) 167ms |
| Fade | linear 83ms |
Scrollbar: expand 167ms, color 83ms, expand delay 400ms, contract delay 500ms.
Window (derived): open scale .9→1 + fade 250ms decel; close fade 167ms accel; minimize point-to-point 250ms.

## A8. Controls [src]
- Button: ControlFill Def/Sec/Ter; elevation-gradient border; pressed flat stroke.
- CheckBox: r4; unchecked 1px StrongStroke + AltFill Sec; checked AccentFill.
- Radio: 20 outer, glyph 12 / 14 hover / 10 pressed.
- ToggleSwitch: track 40x20 r10 1px; knob 12 / 14 hover / 17x14 pressed (83ms). On: accent + white knob.
- Slider: track 4, thumb 18 with 12 inner accent circle.
- ScrollBar: size 12, thumb min 30, thin at rest, expand on hover.
- Focus: outer 2px FocusStrokeOuter + inner 1px FocusStrokeInner, margin -3.
- Context menu: r8, 1px SurfaceStrokeFlyout, acrylic; items r4 inset 4px; hover SubtleFill Sec.
- Tooltip: r4, 1px SurfaceStrokeFlyout, acrylic, 12px text.

## A9. Windows 12
Not announced (Oct 2026). Concept traits: floating rounded taskbar, top status bar, widgets in Start, heavier glass, more rounding.

## A10. Shell dims
Start / Quick Settings / Notification Center / Snap Layouts sizes unpublished — measure from screenshots. QS + NC = 8px acrylic flyouts anchored to taskbar; Snap Layouts = hover popup on maximize.

## B. Existing KDE Win11 themes — weaknesses
| Project | Engine | Problems |
|---|---|---|
| Fluent-kde (vinceliuice) | Aurorae + Kvantum + Plasma SVGZ + LnF | legacy metadata, Qt6 issues, can't follow color scheme, variant sprawl, PNG heavy |
| Fluent-gtk | SCSS | separate tokens → GTK/Qt drift |
| Win11OS-kde (yeyushengfan258) | Aurorae + Kvantum + SVG | Plasma-5 era, corner mismatch, "looks abandoned" |
| Win12OS-kde | Aurorae + Kvantum | stale 2023, dark only |
| Win11 icons | Yaru-derived, not Fluent | broken tray icons on Plasma 6 |
| Kvantum | QtWidgets only | no QML/Plasma, no overlay scrollbars, no acrylic |
| Klassy | C++ KDecoration3 + style | no Mica/Acrylic, no Fluent geometry; rebuild per Plasma |

Cross-cutting: disjoint stacks with separately authored constants → corner/edge mismatch; hardcoded colors; no elevation gradient; no 4/8 radius hierarchy; blur/shadow baked into SVG.
**Our edge:** single token source → every engine generated; native C++ style/decoration/effect; per-surface Acrylic/Mica.

## C. Licenses
| Asset | License | Use |
|---|---|---|
| WinUI XAML tokens | MIT | values reusable |
| Fluent System Icons | MIT | glyph SVGs reusable with notice |
| Fluent 2 tokens | MIT | reusable |
| Segoe UI Variable / Segoe Fluent Icons | proprietary | never bundle; name reference only |
| Selawik | OFL 1.1 | bundle OK |
| Windows logo/name | trademark | avoid |

Plasma 6.8: Wayland-only; **Union** style engine in beta (CSS input, Qt Quick + Widgets, Qt ≥6.9); Plasma shell SVG not styled by Union.

## URLs
- https://github.com/microsoft/microsoft-ui-xaml/tree/main/controls/dev/CommonStyles
- https://github.com/microsoft/microsoft-ui-xaml/blob/main/controls/dev/Materials/Acrylic/AcrylicBrush.cpp
- https://github.com/MicrosoftDocs/windows-dev-docs/tree/docs/hub/apps/design
- https://github.com/microsoft/fluentui/tree/master/packages/tokens/src
- https://github.com/microsoft/fluentui-system-icons
- https://invent.kde.org/plasma/union
- https://kde.org/announcements/plasma/6/6.8.0/
- https://github.com/paulmcauley/klassy
