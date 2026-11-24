//! KConfigXT schema for ~/.config/lumenrc (Lumen System Settings module); defaults come from the tokens.
use crate::ctx::Ctx;
use crate::tokens::{Mode, Tokens};
use anyhow::Result;
use std::fmt::Write;

/// Apps that render video/GL/canvas content or manage their own translucency.
pub const OPAQUE_APPS: &str = "konsole,yakuake,kitty,mpv,vlc,smplayer,haruna,obs,krita,blender,kdenlive,shotcut,steam,virtualbox,qemu,wine";

enum Ty {
    Bool(bool),
    Int(i64, i64, i64),
    Str(&'static str),
}

fn groups(t: &Tokens) -> Vec<(&'static str, Vec<(&'static str, Ty)>)> {
    let pct = |v: f64| (v * 100.0).round() as i64;
    vec![
        ("Appearance", vec![("Mode", Ty::Str("auto"))]),
        ("Fonts", vec![("TextSize", Ty::Int(t.i("font.text_size"), 100, 225))]),
        (
            "Materials",
            vec![
                ("Enabled", Ty::Bool(true)),
                ("AcrylicStrength", Ty::Int(t.i("material.acrylic.strength"), 1, 15)),
                ("AcrylicNoise", Ty::Int(t.i("material.acrylic.noise"), 0, 14)),
                ("AcrylicSaturation", Ty::Int(t.i("material.acrylic.saturation"), 100, 300)),
                ("MicaStrength", Ty::Int(t.i("material.mica.strength"), 1, 15)),
                ("MicaSaturation", Ty::Int(t.i("material.mica.saturation"), 100, 250)),
                ("MicaForPanels", Ty::Bool(true)),
                ("WallpaperTone", Ty::Int(t.i("material.mica.wallpaper_tone"), 0, 100)),
            ],
        ),
        (
            "Decoration",
            vec![
                ("TranslucentTitlebar", Ty::Bool(true)),
                ("TitlebarOpacity", Ty::Int(pct(t.fm("surface.titlebar_alpha_active", Mode::Dark)), 0, 100)),
                ("CornerRadius", Ty::Int(t.i("radius.window"), 0, 24)),
                ("TitlebarHeight", Ty::Int(t.i("size.titlebar"), 20, 64)),
                ("CenterTitle", Ty::Bool(false)),
                ("ShowIcon", Ty::Bool(true)),
                ("ButtonLayout", Ty::Int(0, 0, 2)),
            ],
        ),
        (
            "Style",
            vec![
                ("Animations", Ty::Bool(true)),
                ("AnimationDuration", Ty::Int(100, 25, 400)),
                ("ScrollbarExpanded", Ty::Bool(false)),
                ("TranslucentMenus", Ty::Bool(true)),
                ("TranslucentWindows", Ty::Bool(true)),
                ("WindowOpacity", Ty::Int(0, 0, 100)),
                ("OpaqueApps", Ty::Str(OPAQUE_APPS)),
                ("ControlRadius", Ty::Int(t.i("radius.control"), 0, 12)),
            ],
        ),
        (
            "Animations",
            vec![
                ("WindowOpen", Ty::Bool(true)),
                ("Minimize", Ty::Bool(true)),
                ("OpenScale", Ty::Int(pct(t.f("motion.window_open_scale")), 50, 100)),
                ("WindowDuration", Ty::Int(100, 25, 400)),
            ],
        ),
        (
            "Files",
            vec![
                ("CommandBar", Ty::Bool(true)),
                ("OpenTo", Ty::Int(0, 0, 3)),
                ("ThisPcFreeSpace", Ty::Bool(true)),
                ("ThisPcFolders", Ty::Bool(false)),
                ("ThisPcNetwork", Ty::Bool(true)),
                ("ThisPcHiddenDrives", Ty::Bool(false)),
            ],
        ),
        (
            "LockScreen",
            vec![("LumenWallpaper", Ty::Bool(false)), ("AlwaysShowClock", Ty::Bool(true)), ("MediaControls", Ty::Bool(true))],
        ),
    ]
}

pub enum Bar {
    Int(i64),
    Bool(bool),
}

/// Taskbar settings (Lumen settings → Taskbar) live in the panel: [setting, widget or "panel", widget key, default].
/// lumenrc [Taskbar] holds only the user's choices; the Defaults button and `lumen apply` write the rest.
pub fn taskbar(t: &Tokens) -> Vec<(&'static str, &'static str, &'static str, Bar)> {
    use Bar::{Bool, Int};
    vec![
        ("height", "panel", "height", Int(t.i("size.taskbar_h"))),
        ("floating", "panel", "floating", Bool(false)),
        ("autoHide", "panel", "hiding", Bool(false)),
        ("centered", "org.lumen.spacer", "expanding", Bool(true)),
        ("startIcon", "org.lumen.start", "iconSize", Int(t.i("size.start_icon"))),
        ("appIcon", "org.lumen.tasks", "iconSize", Int(t.i("size.taskbar_icon"))),
        ("tasksSpacing", "org.lumen.tasks", "iconSpacing", Int(0)),
        ("trayIcon", "org.lumen.tray", "iconSize", Int(t.i("size.tray_icon"))),
        ("quickIcon", "org.lumen.tray", "quickIconSize", Int(t.i("size.qs_icon"))),
        ("traySpacing", "org.lumen.tray", "iconSpacing", Int(0)),
        ("clockMode", "org.lumen.clock", "displayMode", Int(0)),
        ("clockDateStyle", "org.lumen.clock", "dateStyle", Int(0)),
        ("clockHour", "org.lumen.clock", "hourFormat", Int(0)),
        ("clockSeconds", "org.lumen.clock", "showSeconds", Bool(false)),
        ("clockBold", "org.lumen.clock", "boldTime", Bool(false)),
        ("clockTimeSize", "org.lumen.clock", "timeSize", Int(100)),
        ("clockDateSize", "org.lumen.clock", "dateSize", Int(100)),
        ("clockTimeAlign", "org.lumen.clock", "timeAlign", Int(2)),
        ("clockDateAlign", "org.lumen.clock", "dateAlign", Int(2)),
    ]
}

/// Plasma script defining lumenPanel(), readBar(panel) and writeBar(panel, values); writeBar only touches what differs.
pub fn taskbar_js(t: &Tokens) -> String {
    let rows: Vec<String> = taskbar(t)
        .iter()
        .map(|(key, plugin, config, d)| {
            let d = match d {
                Bar::Int(n) => n.to_string(),
                Bar::Bool(b) => b.to_string(),
            };
            format!("['{key}','{plugin}','{config}',{d}]")
        })
        .collect();
    format!(
        "var barKeys = [{}];\
         function lumenPanel() {{ var ps = panels(); for (var i = 0; i < ps.length; ++i) {{ if (ps[i].widgets('org.lumen.start').length || ps[i].widgets('org.lumen.tray').length) return ps[i]; }} return null; }}\
         function barWidget(p, plugin) {{ var w = p.widgets(plugin).sort(function (a, b) {{ return a.index - b.index; }}); if (!w.length) return null; w[0].currentConfigGroup = ['General']; return w[0]; }}\
         function readBar(p) {{ var r = {{}}; for (var i = 0; i < barKeys.length; ++i) {{ var k = barKeys[i], v;\
           if (k[1] == 'panel') v = k[2] == 'height' ? p.height : k[2] == 'floating' ? p.floating : p.hiding == 'autohide';\
           else {{ var w = barWidget(p, k[1]); if (!w) continue; v = w.readConfig(k[2], k[3]); if (k[0] == 'clockMode' && String(w.readConfig('showDate', true)) == 'false' && Number(v) != 3) v = 2; }}\
           r[k[0]] = typeof k[3] === 'boolean' ? String(v) == 'true' : Number(v); }} return r; }}\
         function writeBar(p, v) {{ var now = readBar(p), touched = {{}}; for (var i = 0; i < barKeys.length; ++i) {{ var k = barKeys[i];\
           if (!(k[0] in v) || !(k[0] in now) || now[k[0]] === v[k[0]]) continue;\
           if (k[1] == 'panel') {{ if (k[2] == 'height') p.height = v.height; else if (k[2] == 'floating') p.floating = v.floating; else p.hiding = v.autoHide ? 'autohide' : 'none'; continue; }}\
           var w = barWidget(p, k[1]); w.writeConfig(k[2], v[k[0]]); if (k[0] == 'clockMode') w.writeConfig('showDate', true); if (k[0] == 'centered' && !v.centered) w.writeConfig('length', 0); touched[k[1]] = true; }}\
           for (var plugin in touched) barWidget(p, plugin).reloadConfig(); }}",
        rows.join(",")
    )
}

pub fn generate(ctx: &Ctx) -> Result<()> {
    let mut x = String::from(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!-- Generated by `lumen gen` from tokens/tokens.toml. -->\n\
         <kcfg xmlns=\"http://www.kde.org/standards/kcfg/1.0\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n\
         \x20     xsi:schemaLocation=\"http://www.kde.org/standards/kcfg/1.0 http://www.kde.org/standards/kcfg/1.0/kcfg.xsd\">\n\
         \x20 <kcfgfile name=\"lumenrc\"/>\n",
    );
    for (group, entries) in groups(&ctx.t) {
        writeln!(x, "  <group name=\"{group}\">").unwrap();
        for (key, ty) in entries {
            let name = format!("{}{key}", group.to_lowercase());
            match ty {
                Ty::Bool(d) => writeln!(x, "    <entry name=\"{name}\" key=\"{key}\" type=\"Bool\"><default>{d}</default></entry>"),
                Ty::Int(d, lo, hi) => writeln!(
                    x,
                    "    <entry name=\"{name}\" key=\"{key}\" type=\"Int\"><default>{d}</default><min>{lo}</min><max>{hi}</max></entry>"
                ),
                Ty::Str(d) => writeln!(x, "    <entry name=\"{name}\" key=\"{key}\" type=\"String\"><default>{d}</default></entry>"),
            }
            .unwrap();
        }
        x.push_str("  </group>\n");
    }
    x.push_str("</kcfg>\n");
    ctx.write_gen("kcfg/lumensettings.kcfg", x)?;
    let mut h = String::from("// Generated by `lumen gen`.\n#pragma once\n\nnamespace Lumen::Taskbar\n{\nstruct Default {\n    const char *key;\n    bool isBool;\n    int value;\n};\ninline constexpr Default defaults[] = {\n");
    for (key, _, _, d) in taskbar(&ctx.t) {
        let (b, v) = match d {
            Bar::Int(n) => (false, n),
            Bar::Bool(on) => (true, on as i64),
        };
        writeln!(h, "    {{\"{key}\", {b}, {v}}},").unwrap();
    }
    write!(h, "}};\ninline constexpr char script[] = R\"lumenjs({})lumenjs\";\n}}\n", taskbar_js(&ctx.t)).unwrap();
    ctx.write_gen("cpp/lumentaskbar.h", h)?;
    ctx.write_gen(
        "kcfg/lumensettings.kcfgc",
        "File=lumensettings.kcfg\nClassName=LumenSettings\nMutators=true\nDefaultValueGetters=true\nGenerateProperties=true\nParentInConstructor=true\nNotifiers=true\n",
    )
}
