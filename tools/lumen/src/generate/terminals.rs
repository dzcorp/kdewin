use crate::color::Rgba;
use crate::ctx::Ctx;
use crate::tokens::Mode;
use anyhow::Result;
use std::fmt::Write;

const NAMES: [&str; 8] = ["black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"];

struct Term {
    bg: Rgba,
    fg: Rgba,
    cursor: Rgba,
    sel: Rgba,
    ansi: Vec<Rgba>,
}

pub fn generate(ctx: &Ctx) -> Result<()> {
    for m in Mode::ALL {
        let p = format!("terminal.{}", m.name());
        let term = Term {
            bg: ctx.t.color(&format!("{p}.background")),
            fg: ctx.t.color(&format!("{p}.foreground")),
            cursor: ctx.t.color(&format!("{p}.cursor")),
            sel: ctx.t.color(&format!("{p}.selection")),
            ansi: ctx.t.sv(&format!("{p}.ansi")).iter().map(|s| Rgba::parse(s)).collect::<Result<_>>()?,
        };
        let title = m.title();
        ctx.write(format!("share/konsole/Lumen{title}.colorscheme"), konsole(&term, title)?)?;
        ctx.write(format!("apps/kitty/lumen-{}.conf", m.name()), kitty(&term)?)?;
        ctx.write(format!("apps/alacritty/lumen-{}.toml", m.name()), alacritty(&term)?)?;
    }
    Ok(())
}

fn konsole(t: &Term, title: &str) -> Result<String> {
    let mut s = String::new();
    let faint = |c: Rgba| c.mix(t.bg, 0.35);
    let block = |s: &mut String, name: &str, c: Rgba, intense: Rgba| -> Result<()> {
        writeln!(s, "[{name}]\nColor={}\n\n[{name}Faint]\nColor={}\n\n[{name}Intense]\nColor={}\n", c.kde(), faint(c).kde(), intense.kde())?;
        Ok(())
    };
    block(&mut s, "Background", t.bg, t.bg)?;
    for i in 0..8 {
        block(&mut s, &format!("Color{i}"), t.ansi[i], t.ansi[i + 8])?;
    }
    block(&mut s, "Foreground", t.fg, t.fg)?;
    writeln!(s, "[General]\nAnchor=0.5,0.5\nBlur=true\nColorRandomization=false\nDescription=Lumen {title}\nFillStyle=Tile\nOpacity=0.92\nWallpaper=\nWallpaperFlipType=NoFlip\nWallpaperOpacity=1")?;
    Ok(s)
}

fn kitty(t: &Term) -> Result<String> {
    let mut s = String::new();
    writeln!(s, "foreground {}\nbackground {}\ncursor {}\ncursor_text_color {}", t.fg.hex(), t.bg.hex(), t.cursor.hex(), t.bg.hex())?;
    writeln!(s, "selection_background {}\nselection_foreground none\nurl_color {}", t.sel.hex(), t.cursor.hex())?;
    writeln!(s, "active_tab_background {}\nactive_tab_foreground {}\ninactive_tab_background {}\ninactive_tab_foreground {}", t.bg.hex(), t.fg.hex(), t.bg.mix(t.fg, 0.06).hex(), t.fg.mix(t.bg, 0.4).hex())?;
    for (i, c) in t.ansi.iter().enumerate() {
        writeln!(s, "color{i} {}", c.hex())?;
    }
    Ok(s)
}

fn alacritty(t: &Term) -> Result<String> {
    let mut s = String::new();
    writeln!(s, "[colors.primary]\nbackground = \"{}\"\nforeground = \"{}\"\n", t.bg.hex(), t.fg.hex())?;
    writeln!(s, "[colors.cursor]\ncursor = \"{}\"\ntext = \"{}\"\n", t.cursor.hex(), t.bg.hex())?;
    writeln!(s, "[colors.selection]\nbackground = \"{}\"\ntext = \"CellForeground\"\n", t.sel.hex())?;
    for (section, off) in [("normal", 0), ("bright", 8)] {
        writeln!(s, "[colors.{section}]")?;
        for (i, n) in NAMES.iter().enumerate() {
            writeln!(s, "{n} = \"{}\"", t.ansi[off + i].hex())?;
        }
        s.push('\n');
    }
    Ok(s)
}
