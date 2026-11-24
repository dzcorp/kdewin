//! Panel migration for `lumen apply --layout`: snapshot the current bottom panel before the
//! layout reset, hand it to the L&F layout script via ~/.config/lumen-layoutrc, then move the old
//! tray children's settings (weather location, KDE Connect, …) onto the new Lumen tray.
use anyhow::Result;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

type Group = Vec<String>;

/// Minimal KConfig reader keeping raw (still escaped) values and group order.
pub struct Ini {
    groups: Vec<(Group, Vec<(String, String)>)>,
}

impl Ini {
    pub fn parse(text: &str) -> Self {
        let mut groups: Vec<(Group, Vec<(String, String)>)> = vec![(Vec::new(), Vec::new())];
        for line in text.lines() {
            let line = line.trim_end();
            if line.starts_with('[') && line.ends_with(']') {
                let path = line[1..line.len() - 1].split("][").map(str::to_owned).collect();
                groups.push((path, Vec::new()));
            } else if let Some((k, v)) = line.split_once('=')
                && !line.starts_with('#')
            {
                groups.last_mut().unwrap().1.push((k.trim().to_owned(), v.to_owned()));
            }
        }
        Self { groups }
    }

    pub fn get(&self, path: &[&str], key: &str) -> Option<&str> {
        self.groups.iter().filter(|(g, _)| g.iter().map(String::as_str).eq(path.iter().copied())).find_map(|(_, kv)| kv.iter().find(|(k, _)| k == key).map(|(_, v)| v.as_str()))
    }

    /// All (subpath, key, raw value) under `prefix`.
    pub fn subtree(&self, prefix: &[String]) -> Vec<(Vec<String>, String, String)> {
        let mut out = Vec::new();
        for (g, kv) in &self.groups {
            if g.len() > prefix.len() && g.starts_with(prefix) {
                for (k, v) in kv {
                    out.push((g[prefix.len()..].to_vec(), k.clone(), v.clone()));
                }
            }
        }
        out
    }

    /// Direct child names of `prefix` (e.g. applet ids under [..][Applets]).
    pub fn children(&self, prefix: &[String]) -> Vec<String> {
        let mut out: Vec<String> = Vec::new();
        for (g, _) in &self.groups {
            if g.len() > prefix.len() && g.starts_with(prefix) && !out.contains(&g[prefix.len()]) {
                out.push(g[prefix.len()].clone());
            }
        }
        out
    }
}

fn s(v: &[&str]) -> Vec<String> {
    v.iter().map(|x| (*x).to_owned()).collect()
}

/// Undo KConfig entry escaping (\\ \n \t \r \s \xHH); list escapes like "\," are kept.
pub fn unescape(raw: &str) -> String {
    let mut out = String::with_capacity(raw.len());
    let mut it = raw.chars().peekable();
    while let Some(c) = it.next() {
        if c != '\\' {
            out.push(c);
            continue;
        }
        match it.next() {
            Some('\\') => out.push('\\'),
            Some('n') => out.push('\n'),
            Some('t') => out.push('\t'),
            Some('r') => out.push('\r'),
            Some('s') => out.push(' '),
            Some('x') => {
                let hex: String = it.by_ref().take(2).collect();
                match u8::from_str_radix(&hex, 16) {
                    Ok(b) => out.push(b as char),
                    Err(_) => out.push_str(&format!("\\x{hex}")),
                }
            }
            Some(other) => {
                out.push('\\');
                out.push(other);
            }
            None => out.push('\\'),
        }
    }
    out
}

const LAUNCHERS: &[(&str, &str)] = &[
    ("org.kde.plasma.kickoff", "org.kde.plasma.kickoff.favorites.instance-"),
    ("org.kde.plasma.kicker", "org.kde.plasma.kicker.favorites.instance-"),
    ("org.kde.plasma.kickerdash", "org.kde.plasma.kickerdash.favorites.instance-"),
];
const TASKS: &[&str] = &["org.lumen.tasks", "org.kde.plasma.icontasks", "org.kde.plasma.taskmanager"];
/// Replaced by Lumen pieces or meaningless in the Lumen panel.
const DROPPED: &[&str] = &[
    "org.kde.plasma.panelspacer",
    "org.lumen.spacer",
    "org.kde.plasma.marginsseparator",
    "org.kde.plasma.digitalclock",
    "org.kde.plasma.systemtray",
    "org.kde.plasma.brightness",
    "org.lumen.start",
    "org.lumen.tray",
    "org.lumen.clock",
    "luisbocanegra.panel.colorizer",
    "luisbocanegra.panelspacer.extended",
];
const TRAY_KEYS: &[(&str, &str)] = &[
    ("extraItems", "trayExtraItems"),
    ("knownItems", "trayKnownItems"),
    ("shownItems", "trayShownItems"),
    ("hiddenItems", "trayHiddenItems"),
    ("disabledStatusNotifiers", "trayDisabledSNIs"),
];

#[derive(Default)]
pub struct Snapshot {
    pub layout: BTreeMap<String, String>,
    pub widgets: Vec<Vec<(String, String)>>,
    /// Old tray children: plugin -> (subpath, key, raw value) under its Configuration group.
    pub tray_children: Vec<(String, Vec<(Vec<String>, String, String)>)>,
}

/// Plugins from `wanted` found in panels that hold the Lumen Start menu.
pub fn superseded_in_lumen_panels<'a>(path: &Path, wanted: impl Iterator<Item = &'a str> + Clone) -> Vec<String> {
    let Ok(text) = std::fs::read_to_string(path) else { return Vec::new() };
    let ini = Ini::parse(&text);
    let mut found = Vec::new();
    for c in ini.children(&s(&["Containments"])) {
        let plugins: Vec<String> = ini
            .children(&s(&["Containments", &c, "Applets"]))
            .iter()
            .filter_map(|a| ini.get(&["Containments", &c, "Applets", a], "plugin").map(str::to_owned))
            .collect();
        if plugins.iter().any(|p| p == "org.lumen.start") {
            found.extend(plugins.into_iter().filter(|p| wanted.clone().any(|w| w == p)));
        }
    }
    found.sort();
    found.dedup();
    found
}

pub fn appletsrc() -> PathBuf {
    crate::install::config_home().join("plasma-org.kde.plasma.desktop-appletsrc")
}

pub fn snapshot(path: &Path) -> Snapshot {
    let mut snap = Snapshot::default();
    let Ok(text) = std::fs::read_to_string(path) else { return snap };
    let ini = Ini::parse(&text);
    let containments = ini.children(&s(&["Containments"]));
    let panels: Vec<&str> = containments.iter().map(String::as_str).filter(|c| ini.get(&["Containments", c], "plugin") == Some("org.kde.panel")).collect();
    let Some(panel) = panels.iter().find(|c| ini.get(&["Containments", c], "location") == Some("4")).or(panels.first()).copied() else {
        return snap;
    };
    let applets_path = s(&["Containments", panel, "Applets"]);
    let mut order: Vec<String> = ini
        .get(&["Containments", panel, "General"], "AppletOrder")
        .map(|o| o.split(';').filter(|x| !x.is_empty()).map(str::to_owned).collect())
        .unwrap_or_default();
    for id in ini.children(&applets_path) {
        if !order.contains(&id) {
            order.push(id);
        }
    }

    let mut side = "leftWidgets";
    let mut sides: BTreeMap<&str, Vec<String>> = BTreeMap::new();
    for id in order {
        let id = id.as_str();
        let Some(plugin) = ini.get(&["Containments", panel, "Applets", id], "plugin") else { continue };
        let conf = |group: &str, key: &str| ini.get(&["Containments", panel, "Applets", id, "Configuration", group], key).map(unescape);
        let shortcut = ini.get(&["Containments", panel, "Applets", id, "Shortcuts"], "global").map(unescape);
        if let Some((_, client)) = LAUNCHERS.iter().find(|(p, _)| *p == plugin) {
            snap.layout.insert("startFavoritesClient".into(), format!("{client}{id}"));
            if let Some(icon) = conf("General", "icon") {
                snap.layout.insert("startIcon".into(), icon);
            }
            if let Some(sc) = shortcut {
                snap.layout.insert("startShortcut".into(), sc);
            }
            continue;
        }
        if TASKS.contains(&plugin) {
            if let Some(l) = conf("General", "launchers") {
                snap.layout.insert("launchers".into(), l);
            }
            side = "rightWidgets";
            continue;
        }
        if plugin == "org.kde.plasma.systemtray" {
            for (key, dst) in TRAY_KEYS {
                if let Some(v) = ini.get(&["Containments", panel, "Applets", id, "General"], key) {
                    snap.layout.insert((*dst).into(), unescape(v));
                }
            }
            let kids = s(&["Containments", panel, "Applets", id, "Applets"]);
            for child in ini.children(&kids) {
                let base = [kids.clone(), vec![child.clone()]].concat();
                let Some(p) = ini.get(&base.iter().map(String::as_str).collect::<Vec<_>>(), "plugin") else { continue };
                let cfg = ini.subtree(&[base, vec!["Configuration".into()]].concat());
                let cfg: Vec<_> = cfg.into_iter().filter(|(g, _, _)| g.first().is_none_or(|x| x != "ConfigDialog")).collect();
                if !cfg.is_empty() {
                    snap.tray_children.push((p.to_owned(), cfg));
                }
            }
            continue;
        }
        if plugin == "org.kde.plasma.digitalclock" {
            if conf("Appearance", "dateFormat").as_deref() == Some("custom")
                && let Some(f) = conf("Appearance", "customDateFormat")
            {
                snap.layout.insert("clockDateFormat".into(), f);
            }
            if let Some(sec) = conf("Appearance", "showSeconds") {
                snap.layout.insert("clockShowSeconds".into(), if sec == "0" || sec == "false" { "false" } else { "true" }.into());
            }
            continue;
        }
        if DROPPED.contains(&plugin) {
            continue;
        }
        let mut w = vec![("plugin".to_owned(), plugin.to_owned())];
        let cfg = ini.subtree(&s(&["Containments", panel, "Applets", id, "Configuration"]));
        let mut keys = Vec::new();
        for (g, k, v) in cfg {
            if g.first().is_some_and(|x| x == "ConfigDialog") || g.is_empty() {
                continue;
            }
            let full = format!("{}/{k}", g.join("/"));
            keys.push(full.clone());
            w.push((full, v));
        }
        w.push(("keys".into(), keys.join("\u{1f}")));
        if let Some(sc) = shortcut {
            w.push(("shortcut".into(), sc));
        }
        sides.entry(side).or_default().push(snap.widgets.len().to_string());
        snap.widgets.push(w);
    }
    for (k, v) in sides {
        snap.layout.insert(k.into(), v.join(","));
    }
    let view = format!("Panel {panel}");
    if let Some(h) = ini.get(&["PlasmaViews", view.as_str()], "panelVisibility") {
        snap.layout.insert("hiding".into(), match h { "1" => "autohide", "2" => "windowscover", "3" => "windowsgobelow", "4" => "dodgewindows", _ => "none" }.into());
    }
    snap
}

fn escape(v: &str) -> String {
    let mut out = String::with_capacity(v.len());
    for (i, c) in v.chars().enumerate() {
        match c {
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\t' => out.push_str("\\t"),
            '\r' => out.push_str("\\r"),
            '\u{1f}' => out.push_str("\\x1f"),
            ' ' if i == 0 || i + 1 == v.len() => out.push_str("\\s"),
            _ => out.push(c),
        }
    }
    out
}

/// lumenrc text for the layout script. Widget values stay raw (already KConfig-escaped).
pub fn render(snap: &Snapshot) -> String {
    let mut out = String::from("[Layout]\n");
    for (k, v) in &snap.layout {
        out.push_str(&format!("{k}={}\n", escape(v)));
    }
    for (i, w) in snap.widgets.iter().enumerate() {
        out.push_str(&format!("\n[LayoutWidget-{i}]\n"));
        for (k, v) in w {
            let raw = if k == "plugin" || k == "keys" || k == "shortcut" { escape(v) } else { v.clone() };
            out.push_str(&format!("{k}={raw}\n"));
        }
    }
    for (plugin, cfg) in &snap.tray_children {
        out.push_str(&format!("\n# tray child {plugin}: {} settings to migrate\n", cfg.len()));
    }
    out
}

pub fn write_lumenrc(snap: &Snapshot) -> Result<()> {
    crate::ctx::write_file(&crate::install::config_home().join("lumen-layoutrc"), render(snap).as_bytes())
}

/// After the reset: copy old tray children's settings onto the Lumen tray's new children.
/// Returns true when something was written (plasmashell must restart to read it).
pub fn transfer_tray_children(snap: &Snapshot) -> Result<bool> {
    if snap.tray_children.is_empty() {
        return Ok(false);
    }
    let path = appletsrc();
    let deadline = Instant::now() + Duration::from_secs(20);
    let (panel, tray, ini) = loop {
        let ini = Ini::parse(&std::fs::read_to_string(&path).unwrap_or_default());
        let found = ini.children(&s(&["Containments"])).into_iter().find_map(|c| {
            let tray = ini
                .children(&s(&["Containments", c.as_str(), "Applets"]))
                .into_iter()
                .find(|a| ini.get(&["Containments", c.as_str(), "Applets", a.as_str()], "plugin") == Some("org.lumen.tray"))?;
            Some((c, tray))
        });
        match found {
            Some((c, t)) if !ini.children(&s(&["Containments", c.as_str(), "Applets", t.as_str(), "Applets"])).is_empty() => break (c, t, ini),
            _ if Instant::now() > deadline => {
                println!("layout: Lumen tray not saved yet; tray widget settings not migrated");
                return Ok(false);
            }
            _ => std::thread::sleep(Duration::from_millis(500)),
        }
    };
    let kids = s(&["Containments", panel.as_str(), "Applets", tray.as_str(), "Applets"]);
    let mut wrote = false;
    for (plugin, cfg) in &snap.tray_children {
        let Some(child) = ini
            .children(&kids)
            .into_iter()
            .find(|k| ini.get(&["Containments", panel.as_str(), "Applets", tray.as_str(), "Applets", k.as_str()], "plugin") == Some(plugin.as_str()))
        else {
            continue;
        };
        for (sub, key, raw) in cfg {
            let mut groups: Vec<String> = [kids.clone(), vec![child.clone(), "Configuration".into()], sub.clone()].concat();
            groups.retain(|g| !g.is_empty());
            let mut args: Vec<String> = vec!["--file".into(), path.to_string_lossy().into_owned()];
            for g in &groups {
                args.push("--group".into());
                args.push(g.clone());
            }
            args.extend(["--key".into(), key.clone(), unescape(raw)]);
            let refs: Vec<&str> = args.iter().map(String::as_str).collect();
            wrote |= crate::install::try_run("kwriteconfig6", &refs);
        }
    }
    Ok(wrote)
}
