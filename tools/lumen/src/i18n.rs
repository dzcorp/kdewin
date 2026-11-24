//! Translations: extract with xgettext, fill from KDE's installed catalogs, compile at build time.
//!
//! Layout: `po/<domain>.pot` templates, `po/<lang>/<domain>.po` catalogs (installed by `ki18n_install`),
//! `po/manual/<lang>.po` hand translations for strings no KDE catalog has.

use anyhow::{Context, Result, bail};
use std::collections::{BTreeMap, HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use crate::ctx::Ctx;

struct Domain {
    name: &'static str,
    dirs: &'static [&'static str],
    /// Catalogs of the upstream component a domain forks or mirrors; their wording wins.
    upstream: &'static [&'static str],
}

const DOMAINS: &[Domain] = &[
    Domain { name: "kcm_lumen", dirs: &["kcm"], upstream: &[] },
    Domain {
        name: "plasma_applet_org.lumen.clock",
        dirs: &["plasma/plasmoids/org.lumen.clock"],
        upstream: &["plasma_applet_org.kde.plasma.digitalclock", "plasma_applet_org.kde.plasma.notifications", "libnotificationmanager"],
    },
    Domain {
        name: "plasma_applet_org.lumen.start",
        dirs: &["plasma/plasmoids/org.lumen.start"],
        upstream: &["plasma_applet_org.kde.plasma.kickoff", "plasma_applet_org.kde.plasma.kicker"],
    },
    Domain {
        name: "plasma_applet_org.lumen.tray",
        dirs: &["plasma/tray"],
        upstream: &[
            "plasma_applet_org.kde.plasma.private.systemtray",
            "plasma_applet_org.kde.plasma.volume",
            "plasma_applet_org.kde.plasma.networkmanagement",
            "plasma_applet_org.kde.plasma.bluetooth",
            "plasma_applet_org.kde.plasma.battery",
            "plasma_applet_org.kde.plasma.mediacontroller",
            "plasma_applet_org.kde.plasma.brightness",
        ],
    },
    Domain { name: "plasma_applet_org.lumen.spacer", dirs: &["plasma/plasmoids/org.lumen.spacer"], upstream: &["plasma_applet_org.kde.plasma.panelspacer"] },
    Domain { name: "plasma_applet_org.lumen.tasks", dirs: &["plasma/tasks"], upstream: &["plasma_applet_org.kde.plasma.taskmanager"] },
    Domain {
        name: "plasma_applet_org.lumen.notifier",
        dirs: &["plasma/notifier"],
        upstream: &["plasma_applet_org.kde.plasma.notifications", "libnotificationmanager"],
    },
    Domain { name: "lumen_dolphin", dirs: &["apps/dolphin/src/lumen"], upstream: &["dolphin", "kio6"] },
];

/// Launchers renamed in every language: (stock launcher, Lumen launcher, domain, name).
const LAUNCHERS: &[(&str, &str, &str, &str)] =
    &[("apps/dolphin/src/org.kde.dolphin.desktop", "apps/dolphin/src/lumen/org.kde.dolphin.desktop", "lumen_dolphin", "File Explorer")];

/// Translated by hand where KDE has no wording; every other KDE language gets the catalog matches only.
pub const FULL_LANGS: &[&str] =
    &["de", "fr", "es", "it", "pt_BR", "ru", "uk", "pl", "nl", "tr", "ja", "ko", "zh_CN", "zh_TW", "ar", "hi", "id", "vi", "cs", "sv"];

const LOCALE: &str = "/usr/share/locale";

// ---------------------------------------------------------------- po files

#[derive(Clone, Default)]
pub struct Entry {
    ctx: Option<String>,
    id: String,
    plural: Option<String>,
    strs: Vec<String>,
    /// Comment lines kept verbatim (`#.`, `#:`), flags excluded.
    notes: Vec<String>,
    fuzzy: bool,
}

impl Entry {
    fn key(&self) -> String {
        let mut k = String::new();
        if let Some(c) = &self.ctx {
            k.push_str(c);
            k.push('\u{4}');
        }
        k.push_str(&self.id);
        if let Some(p) = &self.plural {
            k.push('\0');
            k.push_str(p);
        }
        k
    }

    fn loose_key(&self) -> String {
        let k = self.key();
        k.split_once('\u{4}').map_or(k.clone(), |(_, rest)| rest.to_owned())
    }
}

pub struct Po {
    header: String,
    entries: Vec<Entry>,
}

fn unescape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut chars = s.chars();
    while let Some(c) = chars.next() {
        if c != '\\' {
            out.push(c);
            continue;
        }
        match chars.next() {
            Some('n') => out.push('\n'),
            Some('t') => out.push('\t'),
            Some('r') => out.push('\r'),
            Some(o) => out.push(o),
            None => {}
        }
    }
    out
}

fn escape(s: &str) -> String {
    s.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n").replace('\t', "\\t")
}

fn quoted(line: &str) -> Option<String> {
    let l = line.trim();
    (l.len() >= 2 && l.starts_with('"') && l.ends_with('"')).then(|| unescape(&l[1..l.len() - 1]))
}

impl Po {
    pub fn parse(text: &str) -> Po {
        #[derive(PartialEq)]
        enum Field {
            None,
            Ctx,
            Id,
            Plural,
            Str(usize),
        }
        let mut entries = Vec::new();
        let mut cur = Entry::default();
        let mut field = Field::None;
        let mut started = false;
        let flush = |cur: &mut Entry, started: &mut bool, entries: &mut Vec<Entry>| {
            if *started {
                entries.push(std::mem::take(cur));
            }
            *started = false;
        };
        for line in text.lines() {
            let l = line.trim_end();
            if l.is_empty() {
                flush(&mut cur, &mut started, &mut entries);
                field = Field::None;
            } else if let Some(flags) = l.strip_prefix("#,") {
                if field != Field::None {
                    flush(&mut cur, &mut started, &mut entries);
                    field = Field::None;
                }
                cur.fuzzy = flags.contains("fuzzy");
            } else if l.starts_with("#~") {
                continue;
            } else if l.starts_with('#') {
                if field != Field::None {
                    flush(&mut cur, &mut started, &mut entries);
                    field = Field::None;
                }
                if l.starts_with("#.") || l.starts_with("#:") {
                    cur.notes.push(l.to_owned());
                }
            } else if let Some(rest) = l.strip_prefix("msgctxt ") {
                if matches!(field, Field::Str(_)) {
                    flush(&mut cur, &mut started, &mut entries);
                }
                started = true;
                cur.ctx = Some(quoted(rest).unwrap_or_default());
                field = Field::Ctx;
            } else if let Some(rest) = l.strip_prefix("msgid_plural ") {
                cur.plural = Some(quoted(rest).unwrap_or_default());
                field = Field::Plural;
            } else if let Some(rest) = l.strip_prefix("msgid ") {
                if matches!(field, Field::Str(_)) {
                    flush(&mut cur, &mut started, &mut entries);
                }
                started = true;
                cur.id = quoted(rest).unwrap_or_default();
                field = Field::Id;
            } else if let Some(rest) = l.strip_prefix("msgstr[") {
                let (n, val) = rest.split_once(']').unwrap_or(("0", ""));
                let n: usize = n.parse().unwrap_or(0);
                if cur.strs.len() <= n {
                    cur.strs.resize(n + 1, String::new());
                }
                cur.strs[n] = quoted(val).unwrap_or_default();
                field = Field::Str(n);
            } else if let Some(rest) = l.strip_prefix("msgstr ") {
                cur.strs = vec![quoted(rest).unwrap_or_default()];
                field = Field::Str(0);
            } else if let Some(more) = quoted(l) {
                match field {
                    Field::Ctx => cur.ctx.get_or_insert_default().push_str(&more),
                    Field::Id => cur.id.push_str(&more),
                    Field::Plural => cur.plural.get_or_insert_default().push_str(&more),
                    Field::Str(n) => cur.strs[n].push_str(&more),
                    Field::None => {}
                }
            }
        }
        flush(&mut cur, &mut started, &mut entries);
        let mut header = String::new();
        if let Some(pos) = entries.iter().position(|e| e.id.is_empty() && e.ctx.is_none()) {
            header = entries.remove(pos).strs.into_iter().next().unwrap_or_default();
        }
        Po { header, entries }
    }

    fn write(&self) -> String {
        let mut out = format!("msgid \"\"\nmsgstr \"{}\"\n", escape(&self.header));
        for e in &self.entries {
            out.push('\n');
            for n in &e.notes {
                out.push_str(n);
                out.push('\n');
            }
            if e.fuzzy {
                out.push_str("#, fuzzy\n");
            }
            if let Some(c) = &e.ctx {
                out.push_str(&format!("msgctxt \"{}\"\n", escape(c)));
            }
            out.push_str(&format!("msgid \"{}\"\n", escape(&e.id)));
            match &e.plural {
                Some(p) => {
                    out.push_str(&format!("msgid_plural \"{}\"\n", escape(p)));
                    for (i, s) in e.strs.iter().enumerate() {
                        out.push_str(&format!("msgstr[{i}] \"{}\"\n", escape(s)));
                    }
                }
                None => out.push_str(&format!("msgstr \"{}\"\n", escape(e.strs.first().map_or("", String::as_str)))),
            }
        }
        out
    }
}

fn read_po(path: &Path) -> Option<Po> {
    fs::read_to_string(path).ok().map(|t| Po::parse(&t))
}

// ---------------------------------------------------------------- extraction

const KEYWORDS: &[&str] = &[
    "i18n:1", "i18nc:1c,2", "i18np:1,2", "i18ncp:1c,2,3", "ki18n:1", "ki18nc:1c,2", "ki18np:1,2", "ki18ncp:1c,2,3", "xi18n:1", "xi18nc:1c,2", "xi18np:1,2",
    "xi18ncp:1c,2,3", "kxi18n:1", "kxi18nc:1c,2", "I18N_NOOP:1", "I18NC_NOOP:1c,2",
];

fn sources(root: &Path, dirs: &[&str], exts: &[&str]) -> Vec<PathBuf> {
    let mut files: Vec<PathBuf> = dirs
        .iter()
        .flat_map(|d| walkdir::WalkDir::new(root.join(d)).into_iter().filter_map(Result::ok))
        .map(|e| e.into_path())
        .filter(|p| p.extension().and_then(|e| e.to_str()).is_some_and(|e| exts.contains(&e)))
        .collect();
    files.sort();
    files
}

fn xgettext(root: &Path, lang: &str, files: &[PathBuf], out: &Path, join: bool) -> Result<()> {
    if files.is_empty() {
        return Ok(());
    }
    let mut cmd = Command::new("xgettext");
    cmd.current_dir(root)
        .args(["--from-code=UTF-8", "-k", "--add-comments=i18n", "--sort-by-file", "--no-wrap", "-L", lang, "-o"])
        .arg(out);
    for k in KEYWORDS {
        cmd.arg(format!("--keyword={k}"));
    }
    if join {
        cmd.arg("--join-existing");
    }
    cmd.args(files.iter().map(|f| f.strip_prefix(root).unwrap_or(f)));
    let st = cmd.status().context("xgettext")?;
    if !st.success() {
        bail!("xgettext failed for {}", out.display());
    }
    Ok(())
}

fn extract(ctx: &Ctx) -> Result<Vec<(&'static Domain, Po)>> {
    let po_dir = ctx.root.join("po");
    fs::create_dir_all(&po_dir)?;
    let mut out = Vec::new();
    for d in DOMAINS {
        let pot = po_dir.join(format!("{}.pot", d.name));
        let _ = fs::remove_file(&pot);
        let js = sources(&ctx.root, d.dirs, &["qml", "js"]);
        let cpp = sources(&ctx.root, d.dirs, &["cpp", "h"]);
        xgettext(&ctx.root, "JavaScript", &js, &pot, false)?;
        xgettext(&ctx.root, "C++", &cpp, &pot, pot.exists())?;
        let po = read_po(&pot).unwrap_or(Po { header: String::new(), entries: Vec::new() });
        out.push((d, po));
    }
    Ok(out)
}

// ---------------------------------------------------------------- KDE catalogs

/// Strings of one .mo file whose originals we want: (original, translation), both raw.
fn scan_mo(path: &Path, wanted: &HashSet<String>, loose: &HashSet<String>) -> Vec<(String, String)> {
    let Ok(data) = fs::read(path) else { return Vec::new() };
    let u32_at = |o: usize| data.get(o..o + 4).map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]]) as usize);
    if u32_at(0) != Some(0x950412de) {
        return Vec::new();
    }
    let (Some(n), Some(orig), Some(trans)) = (u32_at(8), u32_at(12), u32_at(16)) else { return Vec::new() };
    let string = |table: usize, i: usize| -> Option<&str> {
        let len = u32_at(table + 8 * i)?;
        let off = u32_at(table + 8 * i + 4)?;
        std::str::from_utf8(data.get(off..off + len)?).ok()
    };
    let mut found = Vec::new();
    for i in 0..n {
        let Some(o) = string(orig, i) else { continue };
        let bare = o.split_once('\u{4}').map_or(o, |(_, rest)| rest);
        if (wanted.contains(o) || loose.contains(bare) || o.is_empty())
            && let Some(t) = string(trans, i)
        {
            found.push((o.to_owned(), t.to_owned()));
        }
    }
    found
}

fn is_kde_catalog(name: &str) -> bool {
    const MARKS: &[&str] = &[
        "plasma", "kcm", "kwin", "kf6", "kio", "kde", "dolphin", "konsole", "kate", "okular", "spectacle", "systemsettings", "breeze", "kscreen", "powerdevil",
        "bluedevil", "kded", "kinfocenter", "kirigami", "libk", "knotif", "kxmlgui", "kwidgets", "kconfig", "kcore", "kitem", "ktext", "kjob", "solid", "baloo",
        "discover", "ark", "gwenview", "elisa", "krunner", "ksmserver", "kglobalaccel", "kactivit", "kcmutils", "libnotificationmanager", "kmenuedit",
    ];
    MARKS.iter().any(|m| name.contains(m))
}

struct Memory {
    plural_forms: String,
    /// original -> [(catalog, translation)]
    exact: HashMap<String, Vec<(String, String)>>,
    /// original without context -> [(catalog, translation)]
    loose: HashMap<String, Vec<(String, String)>>,
}

fn memory(lang: &str, wanted: &HashSet<String>, loose_wanted: &HashSet<String>) -> Memory {
    let dir = Path::new(LOCALE).join(lang).join("LC_MESSAGES");
    let mut mem = Memory { plural_forms: String::new(), exact: HashMap::new(), loose: HashMap::new() };
    let Ok(rd) = fs::read_dir(&dir) else { return mem };
    let mut files: Vec<PathBuf> = rd.filter_map(Result::ok).map(|e| e.path()).filter(|p| p.extension().is_some_and(|e| e == "mo")).collect();
    files.sort();
    for f in files {
        let cat = f.file_stem().and_then(|s| s.to_str()).unwrap_or_default().to_owned();
        for (o, t) in scan_mo(&f, wanted, loose_wanted) {
            if o.is_empty() {
                if mem.plural_forms.is_empty() || cat == "plasmashell" {
                    if let Some(pf) = t.lines().find_map(|l| l.strip_prefix("Plural-Forms:")) {
                        mem.plural_forms = pf.trim().to_owned();
                    }
                }
                continue;
            }
            if t.split('\0').any(str::is_empty) {
                continue;
            }
            let bare = o.split_once('\u{4}').map_or(o.as_str(), |(_, rest)| rest).to_owned();
            if wanted.contains(&o) {
                mem.exact.entry(o).or_default().push((cat.clone(), t.clone()));
            }
            if loose_wanted.contains(&bare) {
                mem.loose.entry(bare).or_default().push((cat.clone(), t));
            }
        }
    }
    mem
}

/// Upstream wording first, then the most common KDE wording, then anything.
fn pick(cands: &[(String, String)], upstream: &[&str]) -> Option<String> {
    if let Some((_, t)) = upstream.iter().find_map(|u| cands.iter().find(|(c, _)| c == u)) {
        return Some(t.clone());
    }
    let mut votes: HashMap<&str, (usize, bool)> = HashMap::new();
    for (c, t) in cands {
        let v = votes.entry(t.as_str()).or_insert((0, false));
        v.0 += 1;
        v.1 |= is_kde_catalog(c);
    }
    votes.into_iter().max_by(|a, b| (a.1.1, a.1.0, std::cmp::Reverse(a.0)).cmp(&(b.1.1, b.1.0, std::cmp::Reverse(b.0)))).map(|(t, _)| t.to_owned())
}

fn langs() -> Vec<String> {
    let mut v: Vec<String> = fs::read_dir(LOCALE)
        .map(|rd| {
            rd.filter_map(Result::ok)
                .filter(|e| e.path().join("LC_MESSAGES/plasmashell.mo").is_file())
                .filter_map(|e| e.file_name().into_string().ok())
                .collect()
        })
        .unwrap_or_default();
    v.sort();
    v
}

// ---------------------------------------------------------------- hand translations

/// `po/manual/strings.tsv` numbers every string once (`n<TAB>key`); `po/manual/<lang>.tsv` holds
/// `n<TAB>translation[<TAB>plural form…]`. Numbers never change, so translations survive re-extraction.
fn tsv_escape(s: &str) -> String {
    s.replace('\\', "\\\\").replace('\n', "\\n").replace('\t', "\\t").replace('\u{4}', "\\4").replace('\0', "\\0")
}

fn tsv_unescape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut chars = s.chars();
    while let Some(c) = chars.next() {
        if c != '\\' {
            out.push(c);
            continue;
        }
        match chars.next() {
            Some('n') => out.push('\n'),
            Some('t') => out.push('\t'),
            Some('4') => out.push('\u{4}'),
            Some('0') => out.push('\0'),
            Some(o) => out.push(o),
            None => {}
        }
    }
    out
}

fn read_tsv(path: &Path) -> BTreeMap<usize, Vec<String>> {
    fs::read_to_string(path)
        .unwrap_or_default()
        .lines()
        .filter_map(|l| {
            let mut cols = l.split('\t');
            let n = cols.next()?.trim().parse().ok()?;
            let vals: Vec<String> = cols.map(tsv_unescape).collect();
            (!vals.is_empty() && vals.iter().all(|v| !v.is_empty())).then_some((n, vals))
        })
        .collect()
}

/// key -> number, appending numbers for keys not seen before.
fn manual_index(po_dir: &Path, keys: &[String]) -> Result<HashMap<String, usize>> {
    let path = po_dir.join("manual/strings.tsv");
    let existing = read_tsv(&path);
    let mut index: HashMap<String, usize> = existing.iter().map(|(n, v)| (v[0].clone(), *n)).collect();
    let mut next = existing.keys().max().map_or(1, |m| m + 1);
    let mut added = String::new();
    for k in keys {
        if !index.contains_key(k) {
            index.insert(k.clone(), next);
            added.push_str(&format!("{next}\t{}\n", tsv_escape(k)));
            next += 1;
        }
    }
    if !added.is_empty() {
        fs::create_dir_all(path.parent().unwrap())?;
        let mut text = fs::read_to_string(&path).unwrap_or_default();
        text.push_str(&added);
        fs::write(&path, text)?;
    }
    Ok(index)
}

// ---------------------------------------------------------------- filling

#[derive(Clone, Copy, PartialEq)]
enum Source {
    Upstream,
    Manual,
    Catalog,
}

fn split(t: &str) -> Vec<String> {
    t.split('\0').map(str::to_owned).collect()
}

/// Upstream wording of the forked component, then the hand translation, then other KDE catalogs, the
/// latter only for phrases of 3+ words: short ones ("Light", "Start", "Floating") are too ambiguous.
fn resolve(t: &Entry, domain: &Domain, mem: &Memory, manual: &HashMap<String, Vec<String>>) -> Option<(Vec<String>, Source)> {
    let upstream_only = |c: &Vec<(String, String)>| c.iter().find(|(cat, _)| domain.upstream.contains(&cat.as_str())).map(|(_, t)| t.clone());
    if let Some(t) = mem.exact.get(&t.key()).and_then(upstream_only).or_else(|| mem.loose.get(&t.loose_key()).and_then(upstream_only)) {
        return Some((split(&t), Source::Upstream));
    }
    if let Some(m) = manual.get(&t.key()) {
        return Some((m.clone(), Source::Manual));
    }
    if t.id.split_whitespace().count() < 3 {
        return None;
    }
    mem.exact
        .get(&t.key())
        .and_then(|c| pick(c, &[]))
        .or_else(|| mem.loose.get(&t.loose_key()).and_then(|c| pick(c, &[])))
        .map(|s| (split(&s), Source::Catalog))
}

struct LangResult {
    lang: String,
    done: usize,
    all: usize,
    /// Numbers of strings with neither upstream nor hand translation.
    todo: Vec<usize>,
}

pub fn run(ctx: &Ctx) -> Result<()> {
    let t0 = std::time::Instant::now();
    let templates = extract(ctx)?;
    let wanted: HashSet<String> = templates.iter().flat_map(|(_, po)| po.entries.iter().map(Entry::key)).collect();
    let loose: HashSet<String> = templates.iter().flat_map(|(_, po)| po.entries.iter().map(Entry::loose_key)).collect();
    let po_dir = ctx.root.join("po");
    let mut keys: Vec<String> = templates.iter().flat_map(|(_, po)| po.entries.iter().map(Entry::key)).collect();
    keys.dedup();
    let index = manual_index(&po_dir, &keys)?;
    let langs = langs();

    let mut results: Vec<LangResult> = std::thread::scope(|s| {
        let handles: Vec<_> = langs
            .iter()
            .map(|lang| {
                let (templates, wanted, loose, po_dir, index) = (&templates, &wanted, &loose, &po_dir, &index);
                s.spawn(move || fill_lang(lang, templates, wanted, loose, po_dir, index))
            })
            .collect();
        handles.into_iter().filter_map(|h| h.join().ok()).filter_map(Result::ok).collect()
    });
    results.sort_by(|a, b| a.lang.cmp(&b.lang));

    write_launchers(&ctx.root, &po_dir, &langs)?;
    println!("{} strings in {} domains, {} languages, {:.1}s", wanted.len(), DOMAINS.len(), langs.len(), t0.elapsed().as_secs_f32());
    for r in &results {
        let full = FULL_LANGS.contains(&r.lang.as_str());
        if full || r.done * 2 >= r.all {
            let pct = 100.0 * r.done as f32 / r.all.max(1) as f32;
            let note = if full && !r.todo.is_empty() { format!("  {} to translate by hand", r.todo.len()) } else { String::new() };
            println!("  {:12} {pct:5.1}%{note}", r.lang);
        }
    }
    Ok(())
}

/// The launcher's [Desktop Entry] Name in every language the catalogs translate; everything else stays stock.
fn write_launchers(root: &Path, po_dir: &Path, langs: &[String]) -> Result<()> {
    for (stock, out, domain, name) in LAUNCHERS {
        let text = fs::read_to_string(root.join(stock)).with_context(|| stock.to_string())?;
        let names: Vec<(&String, String)> = langs
            .iter()
            .filter_map(|lang| {
                let po = read_po(&po_dir.join(lang).join(format!("{domain}.po")))?;
                let t = po.entries.iter().find(|e| e.ctx.is_none() && e.id == *name)?.strs.first()?.clone();
                (!t.is_empty()).then_some((lang, t))
            })
            .collect();
        let mut doc = String::with_capacity(text.len());
        let mut entry = false;
        for line in text.lines() {
            if line.starts_with('[') {
                entry = line == "[Desktop Entry]";
            }
            if entry && (line.starts_with("Name=") || line.starts_with("Name[")) {
                if line.starts_with("Name=") {
                    doc.push_str(&format!("Name={name}\n"));
                    for (lang, t) in &names {
                        doc.push_str(&format!("Name[{lang}]={t}\n"));
                    }
                }
                continue;
            }
            doc.push_str(line);
            doc.push('\n');
        }
        fs::write(root.join(out), doc)?;
    }
    Ok(())
}

fn fill_lang(lang: &str, templates: &[(&Domain, Po)], wanted: &HashSet<String>, loose: &HashSet<String>, po_dir: &Path, index: &HashMap<String, usize>) -> Result<LangResult> {
    let mem = memory(lang, wanted, loose);
    let by_number = read_tsv(&po_dir.join("manual").join(format!("{lang}.tsv")));
    let manual: HashMap<String, Vec<String>> = index.iter().filter_map(|(k, n)| by_number.get(n).map(|v| (k.clone(), v.clone()))).collect();
    let mut result = LangResult { lang: lang.to_owned(), done: 0, all: 0, todo: Vec::new() };
    for (domain, pot) in templates {
        let path = po_dir.join(lang).join(format!("{}.po", domain.name));
        let mut po = Po {
            header: format!(
                "Project-Id-Version: lumen\nPO-Revision-Date: 2026-11-02 00:00+0000\nLast-Translator: Lumen\nLanguage-Team: Lumen\nLanguage: {lang}\nMIME-Version: 1.0\nContent-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: 8bit\nPlural-Forms: {}\n",
                if mem.plural_forms.is_empty() { "nplurals=2; plural=(n != 1);" } else { &mem.plural_forms }
            ),
            entries: Vec::with_capacity(pot.entries.len()),
        };
        let mut translated = 0;
        for t in &pot.entries {
            let mut e = t.clone();
            e.fuzzy = false;
            let found = resolve(t, domain, &mem, &manual);
            if !matches!(found, Some((_, Source::Upstream | Source::Manual))) {
                result.todo.push(index[&t.key()]);
            }
            match found {
                Some((strs, _)) => {
                    e.strs = strs;
                    translated += 1;
                }
                None => e.strs = vec![String::new(); if t.plural.is_some() { 2 } else { 1 }],
            }
            po.entries.push(e);
        }
        result.done += translated;
        result.all += pot.entries.len();
        if translated > 0 {
            fs::create_dir_all(path.parent().unwrap())?;
            fs::write(&path, po.write())?;
        } else {
            let _ = fs::remove_file(&path);
        }
    }
    result.todo.sort_unstable();
    result.todo.dedup();
    Ok(result)
}

/// Strings of one language that need a hand translation, as `n<TAB>source` lines for `po/manual/<lang>.tsv`.
pub fn todo(ctx: &Ctx, lang: &str) -> Result<()> {
    let templates = extract(ctx)?;
    let wanted: HashSet<String> = templates.iter().flat_map(|(_, po)| po.entries.iter().map(Entry::key)).collect();
    let loose: HashSet<String> = templates.iter().flat_map(|(_, po)| po.entries.iter().map(Entry::loose_key)).collect();
    let po_dir = ctx.root.join("po");
    let keys: Vec<String> = templates.iter().flat_map(|(_, po)| po.entries.iter().map(Entry::key)).collect();
    let index = manual_index(&po_dir, &keys)?;
    let r = fill_lang(lang, &templates, &wanted, &loose, &po_dir, &index)?;
    let by_number: HashMap<usize, &String> = index.iter().map(|(k, n)| (*n, k)).collect();
    for n in r.todo {
        let key = by_number[&n];
        let shown = match key.split_once('\u{4}') {
            Some((ctx, id)) => format!("[{ctx}] {id}"),
            None => key.clone(),
        };
        println!("{n}\t{}", tsv_escape(&shown).replace("\\0", " ‖ "));
    }
    Ok(())
}
