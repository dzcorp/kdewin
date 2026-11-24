use crate::color::{Rgba, accent_shades};
use crate::ctx::Ctx;
use crate::tokens::Mode;
use anyhow::Result;
use std::fmt::Write;

struct Set {
    bg: Rgba,
    bg_alt: Rgba,
    fg: Rgba,
    fg_inactive: Rgba,
}

pub fn generate(ctx: &Ctx) -> Result<()> {
    for m in Mode::ALL {
        ctx.write(format!("share/color-schemes/Lumen{}.colors", m.title()), scheme(ctx, m)?)?;
    }
    Ok(())
}

fn scheme(ctx: &Ctx, m: Mode) -> Result<String> {
    let t = &ctx.t;
    let p = |k: &str| t.p(m, k);
    let shades = accent_shades(t, t.color("accent.default"));
    let text = p("text");
    let inactive = p("text_inactive");

    let sets = [
        ("View", Set { bg: p("view"), bg_alt: p("view_alt"), fg: text, fg_inactive: inactive }),
        ("Window", Set { bg: p("window"), bg_alt: p("window_alt"), fg: text, fg_inactive: inactive }),
        ("Button", Set { bg: p("button"), bg_alt: p("button_alt"), fg: text, fg_inactive: inactive }),
        (
            "Selection",
            Set {
                bg: p("selection"),
                bg_alt: p("selection").mix(Rgba::BLACK, 0.15),
                fg: p("selection_text"),
                fg_inactive: p("selection_text").mix(p("selection"), 0.3),
            },
        ),
        ("Tooltip", Set { bg: p("tooltip"), bg_alt: p("tooltip_alt"), fg: text, fg_inactive: inactive }),
        (
            "Complementary",
            Set {
                bg: p("complementary"),
                bg_alt: p("complementary_alt"),
                fg: Rgba::WHITE,
                fg_inactive: Rgba::parse("#CCCCCC")?,
            },
        ),
        ("Header", Set { bg: p("header"), bg_alt: p("header_alt"), fg: text, fg_inactive: inactive }),
        ("Header][Inactive", Set { bg: p("header_inactive"), bg_alt: p("header_alt"), fg: text, fg_inactive: inactive }),
    ];

    let mut s = String::with_capacity(4096);
    s.push_str(
        "[ColorEffects:Disabled]\nChangeSelectionColor=\nColor=56,56,56\nColorAmount=0\nColorEffect=0\nContrastAmount=0.65\nContrastEffect=1\nEnable=\nIntensityAmount=0.1\nIntensityEffect=2\n\n\
         [ColorEffects:Inactive]\nChangeSelectionColor=true\nColor=112,111,110\nColorAmount=0.025\nColorEffect=2\nContrastAmount=0.1\nContrastEffect=2\nEnable=false\nIntensityAmount=0\nIntensityEffect=0\n\n",
    );

    for (name, set) in &sets {
        let comp = *name == "Complementary";
        let (link, visited, active) = if comp {
            (shades.light2, p("link_visited"), shades.light2)
        } else if *name == "Selection" {
            (p("selection_text"), p("selection_text"), p("selection_text"))
        } else {
            (p("link"), p("link_visited"), p("active"))
        };
        writeln!(s, "[Colors:{name}]")?;
        writeln!(s, "BackgroundAlternate={}", set.bg_alt.kde())?;
        writeln!(s, "BackgroundNormal={}", set.bg.kde())?;
        writeln!(s, "DecorationFocus={}", p("focus").kde())?;
        writeln!(s, "DecorationHover={}", p("hover").kde())?;
        writeln!(s, "ForegroundActive={}", active.kde())?;
        writeln!(s, "ForegroundInactive={}", set.fg_inactive.kde())?;
        writeln!(s, "ForegroundLink={}", link.kde())?;
        writeln!(s, "ForegroundNegative={}", p("negative").kde())?;
        writeln!(s, "ForegroundNeutral={}", p("neutral").kde())?;
        writeln!(s, "ForegroundNormal={}", set.fg.kde())?;
        writeln!(s, "ForegroundPositive={}", p("positive").kde())?;
        writeln!(s, "ForegroundVisited={}\n", visited.kde())?;
    }

    let id = format!("Lumen{}", m.title());
    writeln!(s, "[General]\nColorScheme={id}\nName=Lumen {}\nshadeSortColumn=true\n", m.title())?;
    s.push_str("[KDE]\ncontrast=4\nframeContrast=0.2\n\n");
    let header = p("header");
    writeln!(
        s,
        "[WM]\nactiveBackground={}\nactiveBlend={}\nactiveForeground={}\ninactiveBackground={}\ninactiveBlend={}\ninactiveForeground={}",
        header.kde(),
        text.kde(),
        text.kde(),
        p("header_inactive").kde(),
        inactive.kde(),
        t.c(m, "text_tertiary").over(header).kde()
    )?;
    Ok(s)
}
