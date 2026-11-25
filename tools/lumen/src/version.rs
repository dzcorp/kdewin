//! `lumen version`: tokens/tokens.toml holds the release version; generated files take it from there, these copies
//! (compiled plugin metadata, the Rust package) are kept in step here.

use crate::ctx::{Ctx, write_file};
use anyhow::{Context, Result, bail};

const TOML: &str = "\nversion = \"";
const JSON: &str = "\"Version\": \"";
const FILES: &[(&str, &str)] = &[
    ("tokens/tokens.toml", TOML),
    ("tools/lumen/Cargo.toml", TOML),
    ("tools/lumen/Cargo.lock", "name = \"lumen\"\nversion = \""),
    ("kcm/kcm_lumen.json", JSON),
    ("decoration/lumen.json", JSON),
    ("kwin/effects/materials/metadata.json", JSON),
    ("plasma/tray/src/metadata.json", JSON),
    ("plasma/tasks/metadata.json", JSON),
    ("plasma/notifier/metadata.json", JSON),
];

/// Byte range of the first value after `key`.
fn value(text: &str, key: &str) -> Option<std::ops::Range<usize>> {
    let start = text.find(key)? + key.len();
    Some(start..start + text[start..].find('"')?)
}

/// Prints the version, failing when a copy differs; with `set`, writes it everywhere.
pub fn run(ctx: &Ctx, set: Option<&str>) -> Result<()> {
    if let Some(v) = set
        && (v.split('.').count() != 3 || !v.split('.').all(|p| !p.is_empty() && p.bytes().all(|b| b.is_ascii_digit())))
    {
        bail!("version must be MAJOR.MINOR.PATCH, got {v}");
    }
    let want = set.unwrap_or(ctx.version());
    let mut stale = Vec::new();
    for (rel, key) in FILES {
        let path = ctx.root.join(rel);
        let text = std::fs::read_to_string(&path).with_context(|| format!("read {rel}"))?;
        let range = value(&text, key).with_context(|| format!("{rel}: no version"))?;
        if &text[range.clone()] == want {
            continue;
        }
        if set.is_some() {
            write_file(&path, format!("{}{want}{}", &text[..range.start], &text[range.end..]).as_bytes())?;
        } else {
            stale.push(format!("{rel} has {}", &text[range]));
        }
    }
    if !stale.is_empty() {
        bail!("tokens/tokens.toml says {want}, but {}; run `lumen version {want}`", stale.join(", "));
    }
    println!("{want}");
    Ok(())
}
