use crate::color::{Rgba, accent_shades};
use crate::ctx::Ctx;
use crate::svg::n;
use crate::tokens::Mode;
use anyhow::Result;
use std::fmt::Write;

const SIZES: [(u32, u32); 2] = [(3840, 2160), (2560, 1440)];

/// Original procedural "bloom": a fan of gradient petals over a deep glow.
pub fn generate(ctx: &Ctx) -> Result<()> {
    let dir = "share/wallpapers/Lumen";
    let jobs: Vec<(String, String, u32, u32)> = Mode::ALL
        .into_iter()
        .flat_map(|m| {
            let svg = bloom(ctx, m);
            let sub = if m == Mode::Dark { "images_dark" } else { "images" };
            let mut v: Vec<_> = SIZES.iter().map(|&(w, h)| (format!("{dir}/contents/{sub}/{w}x{h}.png"), svg.clone(), w, h)).collect();
            if m == Mode::Dark {
                v.push((format!("{dir}/contents/screenshot.png"), svg, 400, 225));
            }
            v
        })
        .collect();
    std::thread::scope(|scope| {
        let handles: Vec<_> = jobs.iter().map(|(rel, svg, w, h)| scope.spawn(move || ctx.render(rel, svg, *w, *h))).collect();
        handles.into_iter().try_for_each(|j| j.join().expect("wallpaper thread"))
    })?;
    ctx.write(
        format!("{dir}/metadata.json"),
        format!(
            "{{\n    \"KPlugin\": {{\n        \"Authors\": [{{ \"Name\": \"{}\" }}],\n        \"Id\": \"Lumen\",\n        \"License\": \"CC-BY-SA-4.0\",\n        \"Name\": \"Lumen\"\n    }}\n}}\n",
            ctx.t.s("meta.author")
        ),
    )
}

pub fn bloom(ctx: &Ctx, m: Mode) -> String {
    let sh = accent_shades(&ctx.t, ctx.t.color("accent.default"));
    let dark = m == Mode::Dark;
    let (w, h) = (3840.0, 2160.0);
    let (bg0, bg1, glow) = if dark {
        (Rgba::parse("#09121f").unwrap(), Rgba::parse("#020408").unwrap(), sh.dark2)
    } else {
        (Rgba::parse("#eef4fb").unwrap(), Rgba::parse("#cfdff2").unwrap(), sh.light2)
    };
    let (root, tip) = if dark { (sh.dark2, sh.light2) } else { (sh.base, sh.light3) };
    let (cx, cy) = (w * 0.60, h * 0.60);
    let mut s = String::with_capacity(8192);
    write!(
        s,
        r#"<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">
<defs>
<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{bg0}"/><stop offset="1" stop-color="{bg1}"/></linearGradient>
<radialGradient id="glow" cx="{gx}" cy="{gy}" r="0.55"><stop offset="0" stop-color="{glow}" stop-opacity="{go}"/><stop offset="1" stop-color="{glow}" stop-opacity="0"/></radialGradient>
<linearGradient id="petal" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="{root}" stop-opacity="0.95"/><stop offset="0.65" stop-color="{tip}" stop-opacity="0.85"/><stop offset="1" stop-color="{tip}" stop-opacity="0.35"/></linearGradient>
<filter id="soft" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="3"/></filter>
<filter id="haze" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="90"/></filter>
</defs>
<rect width="{w}" height="{h}" fill="url(#bg)"/>
<rect width="{w}" height="{h}" fill="url(#glow)"/>
"#,
        bg0 = bg0.hex(),
        bg1 = bg1.hex(),
        gx = n(cx / w),
        gy = n(cy / h),
        glow = glow.hex(),
        go = if dark { "0.55" } else { "0.5" },
        root = root.hex(),
        tip = tip.hex(),
    )
    .unwrap();

    let petals = 9;
    let blend = if dark { "screen" } else { "multiply" };
    let len = h * 0.55;
    writeln!(s, r#"<g transform="translate({},{})" filter="url(#haze)" opacity="0.6">"#, n(cx), n(cy)).unwrap();
    for i in 0..petals {
        let a = -150.0 + i as f64 * (130.0 / (petals - 1) as f64);
        writeln!(s, r#"<ellipse cx="{}" cy="0" rx="{}" ry="{}" transform="rotate({})" fill="{}"/>"#, n(len * 0.5), n(len * 0.5), n(len * 0.12), n(a), tip.hex()).unwrap();
    }
    s.push_str("</g>\n");
    writeln!(s, r#"<g transform="translate({},{})" filter="url(#soft)">"#, n(cx), n(cy)).unwrap();
    for i in 0..petals {
        let a = -150.0 + i as f64 * (130.0 / (petals - 1) as f64);
        let scale = 1.0 - (i as f64 - (petals as f64 - 1.0) / 2.0).abs() * 0.06;
        let l = len * scale;
        writeln!(
            s,
            r#"<ellipse cx="{}" cy="0" rx="{}" ry="{}" transform="rotate({})" fill="url(#petal)" style="mix-blend-mode:{blend}"/>"#,
            n(l * 0.5),
            n(l * 0.5),
            n(l * 0.13),
            n(a)
        )
        .unwrap();
    }
    s.push_str("</g>\n</svg>\n");
    s
}
