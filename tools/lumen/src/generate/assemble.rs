use super::qml_tokens;
use crate::ctx::Ctx;
use crate::tokens::Tokens;
use anyhow::{Result, bail};
use std::collections::HashSet;
use std::path::{Path, PathBuf};

/// Qt Quick Controls style `org.lumen.desktop` (Windows switches over org.kde.desktop).
pub const QQC2_OUT: &str = "qml/org/lumen/desktop";
/// Kirigami component overrides for that style (Kirigami looks up `styles/<Qt Quick Controls style>/`).
pub const KIRIGAMI_STYLE_OUT: &str = "qml/org/kde/kirigami/styles/org.lumen.desktop";

const TEXT: &[&str] = &["qml", "js", "json", "svg", "conf", "desktop", "xml", "txt", "css", "colors", "theme", "plymouth", "script", "ini"];

pub fn generate(ctx: &Ctx) -> Result<()> {
    let tokens_qml = qml_tokens(&ctx.t);
    ctx.write_gen("qml/LumenTokens.qml", &tokens_qml)?;
    let lnf = ctx.src("plasma/look-and-feel");
    if lnf.is_dir() {
        for variant in ["dark", "light"] {
            let dst = format!("share/plasma/look-and-feel/org.lumen.desktop.{variant}");
            clear(ctx, &dst)?;
            copy_tree(ctx, &lnf.join("common"), &dst, &tokens_qml)?;
            copy_tree(ctx, &lnf.join(variant), &dst, &tokens_qml)?;
            let shot = ctx.out_path(format!("share/wallpapers/Lumen/contents/images{}/2560x1440.png", if variant == "dark" { "_dark" } else { "" }));
            if let Ok(png) = std::fs::read(&shot) {
                ctx.write(format!("{dst}/contents/previews/fullscreenpreview.jpg"), &png)?;
            }
            if let Ok(png) = std::fs::read(ctx.out_path("share/wallpapers/Lumen/contents/screenshot.png")) {
                ctx.write(format!("{dst}/contents/previews/preview.png"), &png)?;
            }
        }
    }
    for (src, dst) in [("qqc2", QQC2_OUT), ("kirigami-style", KIRIGAMI_STYLE_OUT)] {
        let dir = ctx.src(src);
        if dir.is_dir() {
            clear(ctx, dst)?;
            copy_tree(ctx, &dir, dst, &tokens_qml)?;
        }
    }
    for (src, dst) in [("plasma/plasmoids", "share/plasma/plasmoids"), ("kwin/effects-js", "share/kwin/effects"), ("plasma/shell-overlay", "system/shell-overlay")] {
        let dir = ctx.src(src);
        if !dir.is_dir() {
            continue;
        }
        for pkg in std::fs::read_dir(&dir)?.filter_map(|e| e.ok()).filter(|e| e.path().is_dir()) {
            let pkg_dst = format!("{dst}/{}", pkg.file_name().to_string_lossy());
            clear(ctx, &pkg_dst)?;
            copy_tree(ctx, &pkg.path(), &pkg_dst, &tokens_qml)?;
        }
    }
    Ok(())
}

/// Packages are rebuilt from scratch so files deleted from the source don't linger in the output.
fn clear(ctx: &Ctx, dst: &str) -> Result<()> {
    match std::fs::remove_dir_all(ctx.out_path(dst)) {
        Err(e) if e.kind() != std::io::ErrorKind::NotFound => Err(e.into()),
        _ => Ok(()),
    }
}

fn copy_tree(ctx: &Ctx, src: &Path, dst: &str, tokens_qml: &str) -> Result<()> {
    if !src.is_dir() {
        return Ok(());
    }
    let mut token_dirs: HashSet<PathBuf> = HashSet::new();
    for entry in walkdir::WalkDir::new(src).into_iter().filter_map(|e| e.ok()).filter(|e| e.file_type().is_file()) {
        let rel = entry.path().strip_prefix(src)?;
        let out = Path::new(dst).join(rel);
        let ext = rel.extension().and_then(|e| e.to_str()).unwrap_or("");
        let bytes = std::fs::read(entry.path())?;
        // Extensionless files (L&F `defaults`, `metadata`) are config text too.
        match (TEXT.contains(&ext) || ext.is_empty()).then(|| String::from_utf8(bytes)) {
            Some(Ok(text)) => {
                if ext == "qml" && text.contains("LumenTokens") {
                    token_dirs.insert(out.parent().unwrap_or(Path::new(dst)).to_path_buf());
                }
                ctx.write(&out, substitute(&ctx.t, &text, entry.path())?)?;
            }
            Some(Err(e)) => ctx.write(&out, e.into_bytes())?,
            None => unreachable!(),
        }
    }
    for dir in token_dirs {
        ctx.write(dir.join("LumenTokens.qml"), tokens_qml)?;
    }
    Ok(())
}

/// Replace `@@token.path@@` with the token's text value.
pub fn substitute(t: &Tokens, text: &str, origin: &Path) -> Result<String> {
    if !text.contains("@@") {
        return Ok(text.to_owned());
    }
    let mut out = String::with_capacity(text.len());
    let mut rest = text;
    while let Some(i) = rest.find("@@") {
        out.push_str(&rest[..i]);
        let after = &rest[i + 2..];
        let Some(j) = after.find("@@") else { bail!("{}: unterminated @@", origin.display()) };
        let key = &after[..j];
        match t.placeholder(key) {
            Some(v) => out.push_str(&v),
            None => bail!("{}: unknown token @@{key}@@", origin.display()),
        }
        rest = &after[j + 2..];
    }
    out.push_str(rest);
    Ok(out)
}
