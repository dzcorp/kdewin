//! `lumen upstream`: keeps the forked KDE sources listed in upstream/forks.toml on the release the distribution ships.
//! Each file merges three-way: the recorded release is the base, the fork ours, the new release theirs.

use crate::ctx::{Ctx, write_file};
use anyhow::{Context, Result, bail};
use std::collections::BTreeSet;
use std::fmt::Write as _;
use std::path::{Path, PathBuf};
use std::process::Command;

const MANIFEST: &str = "upstream/forks.toml";

struct Mapping {
    local: String,
    package: String,
    upstream: String,
    /// Paths (relative to the mapping) rewritten here: upstream changes are listed for review, never merged.
    exclude: Vec<String>,
}

struct Fork {
    name: String,
    version: String,
    url: String,
    releases: String,
    arch: Option<String>,
    version_files: Vec<String>,
    paths: Vec<Mapping>,
    watch: Vec<Mapping>,
}

#[derive(Default)]
struct Report {
    taken: Vec<String>,
    merged: Vec<String>,
    added: Vec<String>,
    removed: Vec<String>,
    conflicts: Vec<String>,
    kept: Vec<String>,
    review: Vec<String>,
}

fn strings(v: Option<&toml::Value>) -> Vec<String> {
    v.and_then(|v| v.as_array()).map(|a| a.iter().filter_map(|s| s.as_str().map(str::to_owned)).collect()).unwrap_or_default()
}

fn mappings(t: &toml::Table, key: &str) -> Result<Vec<Mapping>> {
    let Some(list) = t.get(key).and_then(|v| v.as_array()) else { return Ok(Vec::new()) };
    list.iter()
        .map(|m| {
            let s = |k: &str| m.get(k).and_then(|v| v.as_str()).map(str::to_owned).with_context(|| format!("{key}: missing `{k}`"));
            Ok(Mapping { local: s("local")?, package: s("package")?, upstream: s("upstream")?, exclude: strings(m.get("exclude")) })
        })
        .collect()
}

fn load(ctx: &Ctx) -> Result<Vec<Fork>> {
    let path = ctx.root.join(MANIFEST);
    let text = std::fs::read_to_string(&path).with_context(|| format!("read {}", path.display()))?;
    let table: toml::Table = toml::from_str(&text).with_context(|| format!("parse {}", path.display()))?;
    table
        .iter()
        .map(|(name, v)| {
            let t = v.as_table().with_context(|| format!("[{name}] is not a table"))?;
            let s = |k: &str| t.get(k).and_then(|v| v.as_str()).map(str::to_owned).with_context(|| format!("[{name}] missing `{k}`"));
            Ok(Fork {
                name: name.clone(),
                version: s("version")?,
                url: s("url")?,
                releases: s("releases")?,
                arch: t.get("arch").and_then(|v| v.as_str()).map(str::to_owned),
                version_files: strings(t.get("version_files")),
                paths: mappings(t, "paths")?,
                watch: mappings(t, "watch")?,
            })
        })
        .collect()
}

fn parse_version(v: &str) -> Vec<u64> {
    v.split('.').map(|p| p.parse().unwrap_or(0)).collect()
}

/// Version of the distribution's package (pacman -Si), without epoch and pkgrel.
fn pacman_version(pkg: &str) -> Option<String> {
    let out = Command::new("pacman").args(["-Si", pkg]).env("LC_ALL", "C").output().ok().filter(|o| o.status.success())?;
    let text = String::from_utf8_lossy(&out.stdout);
    let full = text.lines().find_map(|l| l.strip_prefix("Version").and_then(|r| r.split_once(':')).map(|(_, v)| v.trim().to_owned()))?;
    let no_epoch = full.split_once(':').map_or(full.as_str(), |(_, v)| v);
    Some(no_epoch.rsplit_once('-').map_or(no_epoch, |(v, _)| v).to_owned())
}

/// Newest release directory (`X.Y.Z/`) in a download.kde.org listing.
fn newest_listed(url: &str) -> Result<String> {
    let out = Command::new("curl").args(["-sSfL", "--retry", "3", url]).output().context("spawn curl")?;
    if !out.status.success() {
        bail!("curl {url}: {}", String::from_utf8_lossy(&out.stderr).trim());
    }
    let html = String::from_utf8_lossy(&out.stdout);
    html.split("href=\"")
        .skip(1)
        .filter_map(|s| s.split_once('"').map(|(h, _)| h.trim_end_matches('/')))
        .filter(|h| h.contains('.') && h.chars().all(|c| c.is_ascii_digit() || c == '.'))
        .max_by_key(|h| parse_version(h))
        .map(str::to_owned)
        .with_context(|| format!("no releases listed at {url}"))
}

/// The release to follow: what the distribution ships (forks build against its libraries), else KDE's newest.
fn target(fork: &Fork) -> Result<String> {
    match fork.arch.as_deref().and_then(pacman_version) {
        Some(v) => Ok(v),
        None => newest_listed(&fork.releases),
    }
}

/// Unpacked release tree, cached under gen_dir/upstream.
fn fetch(ctx: &Ctx, fork: &Fork, pkg: &str, version: &str) -> Result<PathBuf> {
    let cache = ctx.gen_dir.join("upstream");
    let dir = cache.join(format!("{pkg}-{version}"));
    if dir.join(".complete").is_file() {
        return Ok(dir);
    }
    std::fs::create_dir_all(&cache)?;
    let url = fork.url.replace("{pkg}", pkg).replace("{v}", version);
    let archive = cache.join(format!("{pkg}-{version}.tar"));
    let st = Command::new("curl").args(["-sSfL", "--retry", "3", "-o"]).arg(&archive).arg(&url).status().context("spawn curl")?;
    if !st.success() {
        bail!("download {url} failed");
    }
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir)?;
    let st = Command::new("tar").arg("-xf").arg(&archive).arg("-C").arg(&dir).args(["--strip-components=1", "--no-same-owner"]).status().context("spawn tar")?;
    let _ = std::fs::remove_file(&archive);
    if !st.success() {
        bail!("unpack {url} failed");
    }
    write_file(&dir.join(".complete"), url.as_bytes())?;
    Ok(dir)
}

/// Relative paths of the files under `root` (one empty path when `root` is a file).
fn files_under(root: &Path) -> BTreeSet<PathBuf> {
    if root.is_file() {
        return BTreeSet::from([PathBuf::new()]);
    }
    walkdir::WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
        .filter_map(|e| e.path().strip_prefix(root).ok().map(Path::to_path_buf))
        .collect()
}

fn under(root: &Path, rel: &Path) -> PathBuf {
    if rel.as_os_str().is_empty() { root.to_path_buf() } else { root.join(rel) }
}

/// Upstream changes between two trees: (relative path, what happened).
fn changes(base: &Path, new: &Path) -> Vec<(PathBuf, &'static str)> {
    let rels: BTreeSet<PathBuf> = files_under(base).union(&files_under(new)).cloned().collect();
    rels.into_iter()
        .filter_map(|rel| match (std::fs::read(under(base, &rel)).ok(), std::fs::read(under(new, &rel)).ok()) {
            (Some(b), Some(n)) if b == n => None,
            (Some(_), Some(_)) => Some((rel, "changed")),
            (None, _) => Some((rel, "added")),
            (_, None) => Some((rel, "removed")),
        })
        .collect()
}

fn merge(ctx: &Ctx, m: &Mapping, base: &Path, new: &Path, labels: [&str; 2], rep: &mut Report) -> Result<()> {
    let local = ctx.root.join(&m.local);
    if !local.exists() {
        bail!("{}: not found (upstream/forks.toml)", m.local);
    }
    let name = |rel: &Path| if rel.as_os_str().is_empty() { m.local.clone() } else { format!("{}/{}", m.local, rel.display()) };
    for (rel, what) in changes(base, new) {
        let (b, n, l) = (under(base, &rel), under(new, &rel), under(&local, &rel));
        let ours = std::fs::read(&l).ok();
        if m.exclude.iter().any(|e| rel.as_path() == Path::new(e)) {
            if ours.is_some() {
                rep.review.push(format!("{} ({what} upstream)", name(&rel)));
            }
            continue;
        }
        let theirs = std::fs::read(&n).ok();
        match ours {
            // New upstream files join directories the fork kept; files the fork dropped stay dropped.
            None => {
                if what == "added" && l.parent().is_some_and(Path::is_dir) {
                    write_file(&l, &theirs.unwrap_or_default())?;
                    rep.added.push(name(&rel));
                }
            }
            Some(o) if Some(&o) == theirs.as_ref() => {}
            Some(o) if what == "removed" => {
                if std::fs::read(&b).ok().as_ref() == Some(&o) {
                    std::fs::remove_file(&l)?;
                    rep.removed.push(name(&rel));
                } else {
                    rep.kept.push(format!("{} (removed upstream, changed here)", name(&rel)));
                }
            }
            Some(_) if what == "added" => rep.kept.push(format!("{} (added upstream too)", name(&rel))),
            Some(o) => {
                if std::fs::read(&b).ok().as_ref() == Some(&o) {
                    write_file(&l, &theirs.unwrap_or_default())?;
                    rep.taken.push(name(&rel));
                    continue;
                }
                let st = Command::new("git")
                    .args(["merge-file", "-L", "lumen", "-L", labels[0], "-L", labels[1]])
                    .arg(&l)
                    .arg(&b)
                    .arg(&n)
                    .status()
                    .context("spawn git merge-file")?;
                match st.code() {
                    Some(0) => rep.merged.push(name(&rel)),
                    Some(1..=127) => rep.conflicts.push(name(&rel)),
                    _ => rep.conflicts.push(format!("{} (binary or unreadable: kept ours)", name(&rel))),
                }
            }
        }
    }
    Ok(())
}

/// Rewrites `version = "…"` in the fork's section of the manifest.
fn record_version(ctx: &Ctx, fork: &str, version: &str) -> Result<()> {
    let path = ctx.root.join(MANIFEST);
    let text = std::fs::read_to_string(&path)?;
    let mut section = String::new();
    let mut done = false;
    let lines: Vec<String> = text
        .lines()
        .map(|l| {
            let t = l.trim();
            if t.starts_with('[') {
                section = t.trim_matches(|c| c == '[' || c == ']').to_owned();
            } else if !done && section == fork && t.starts_with("version") && t.contains('=') {
                done = true;
                return format!("version = \"{version}\"");
            }
            l.to_owned()
        })
        .collect();
    if !done {
        bail!("[{fork}] has no version line in {MANIFEST}");
    }
    write_file(&path, format!("{}\n", lines.join("\n")).as_bytes())
}

fn list(md: &mut String, title: &str, items: &[String]) {
    if items.is_empty() {
        return;
    }
    let _ = writeln!(md, "\n**{title}** ({})\n", items.len());
    for i in items {
        let _ = writeln!(md, "- `{i}`");
    }
}

pub fn check(ctx: &Ctx) -> Result<()> {
    for fork in load(ctx)? {
        let latest = target(&fork)?;
        if parse_version(&latest) > parse_version(&fork.version) {
            println!("{}: {} -> {latest}", fork.name, fork.version);
        } else {
            println!("{}: {} (up to date)", fork.name, fork.version);
        }
    }
    Ok(())
}

/// Returns the number of files left with conflicts.
pub fn sync(ctx: &Ctx, only: Option<&str>, to: Option<&str>, report: Option<&Path>) -> Result<usize> {
    let forks = load(ctx)?;
    if let Some(name) = only
        && !forks.iter().any(|f| f.name == name)
    {
        bail!("no fork [{name}] in {MANIFEST}");
    }
    let mut md = String::new();
    let mut conflicts = 0;
    for fork in forks.iter().filter(|f| only.is_none_or(|n| n == f.name)) {
        let new = match to {
            Some(v) => v.to_owned(),
            None => target(fork)?,
        };
        if parse_version(&new) <= parse_version(&fork.version) {
            println!("{}: {} is current", fork.name, fork.version);
            continue;
        }
        println!("{}: merging {} -> {new}", fork.name, fork.version);
        let mut rep = Report::default();
        for (maps, merge_it) in [(&fork.paths, true), (&fork.watch, false)] {
            for m in maps {
                let base = fetch(ctx, fork, &m.package, &fork.version)?.join(&m.upstream);
                let next = fetch(ctx, fork, &m.package, &new)?.join(&m.upstream);
                if merge_it {
                    let labels = [format!("{}-{}", m.package, fork.version), format!("{}-{new}", m.package)];
                    merge(ctx, m, &base, &next, [&labels[0], &labels[1]], &mut rep)?;
                } else {
                    rep.review.extend(changes(&base, &next).into_iter().map(|(rel, what)| format!("{}/{} ({what} upstream)", m.upstream, rel.display())));
                }
            }
        }
        for rel in &fork.version_files {
            let path = ctx.root.join(rel);
            let text = std::fs::read_to_string(&path).with_context(|| format!("read {rel}"))?;
            write_file(&path, text.replace(&fork.version, &new).as_bytes())?;
        }
        record_version(ctx, &fork.name, &new)?;
        conflicts += rep.conflicts.len();
        let _ = writeln!(md, "### {} {} → {new}", fork.name, fork.version);
        list(&mut md, "Conflicts: markers left in the file", &rep.conflicts);
        list(&mut md, "Merged with local changes", &rep.merged);
        list(&mut md, "Taken from upstream (unchanged here)", &rep.taken);
        list(&mut md, "Added", &rep.added);
        list(&mut md, "Removed", &rep.removed);
        list(&mut md, "Kept ours", &rep.kept);
        list(&mut md, "Rewritten here, review upstream changes by hand", &rep.review);
        md.push('\n');
        println!(
            "{}: {} merged, {} taken, {} added, {} removed, {} conflicts, {} to review",
            fork.name,
            rep.merged.len(),
            rep.taken.len(),
            rep.added.len(),
            rep.removed.len(),
            rep.conflicts.len(),
            rep.review.len()
        );
    }
    if let Some(path) = report {
        write_file(path, md.as_bytes())?;
    }
    Ok(conflicts)
}
