use crate::color::{Rgba, accent_shades};
use crate::ctx::Ctx;
use crate::svg::n;
use crate::tokens::Mode;
use anyhow::Result;
use std::fmt::Write;

const PLY: &str = "system/plymouth/themes/lumen";

pub fn generate(ctx: &Ctx) -> Result<()> {
    plymouth(ctx)?;
    limine(ctx)?;
    vconsole(ctx)
}

/// Angle travelled (degrees, 0..720) by a boot dot at evenly spaced fractions of its life: the time to cover each step
/// is inversely proportional to the speed at that bearing, so the dot crawls across the top and rushes through the bottom.
pub fn spin_path(t: &crate::tokens::Tokens, samples: usize) -> Vec<f64> {
    const STEPS: usize = 8192;
    let rush = t.f("boot.spinner_rush");
    let step = 720.0 / STEPS as f64;
    let mut time = Vec::with_capacity(STEPS + 1);
    time.push(0.0);
    for i in 0..STEPS {
        let bearing = (180.0 + (i as f64 + 0.5) * step).to_radians();
        let speed = (rush * (1.0 - bearing.cos()) / 2.0).exp();
        time.push(time[i] + step / speed);
    }
    let total = time[STEPS];
    let mut out = Vec::with_capacity(samples);
    let mut i = 0;
    for s in 0..samples {
        let target = total * s as f64 / (samples - 1) as f64;
        while i + 1 < STEPS && time[i + 1] < target {
            i += 1;
        }
        let span = time[i + 1] - time[i];
        let frac = if span > 0.0 { ((target - time[i]) / span).clamp(0.0, 1.0) } else { 0.0 };
        out.push((i as f64 + frac) * step);
    }
    out
}

/// Angle of a dot at fraction `u` of its life, or None while it is not on screen.
fn spin_angle(path: &[f64], u: f64) -> Option<f64> {
    if !(0.0..=1.0).contains(&u) {
        return None;
    }
    let x = u * (path.len() - 1) as f64;
    let i = (x as usize).min(path.len() - 2);
    Some(path[i] + (path[i + 1] - path[i]) * (x - i as f64))
}

fn plymouth(ctx: &Ctx) -> Result<()> {
    let t = &ctx.t;
    let size = t.f("boot.spinner_size");
    let frames = t.i("boot.spinner_frames") as usize;
    let dots = t.i("boot.spinner_dots") as usize;
    let color = t.color("boot.spinner").hex();
    let bgrt = t.s("boot.logo") == "bgrt";

    let px = (size * 2.0) as u32;
    let path = spin_path(t, 512);
    let (life, lag, trail) = (t.f("boot.spinner_life"), t.f("boot.spinner_lag"), t.f("boot.spinner_trail"));
    let c = size / 2.0;
    let ring = size * 0.42;
    let dot = ring * 0.152;
    for f in 0..frames {
        let mut svg = format!(r#"<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{0}" viewBox="0 0 {0} {0}">"#, n(size));
        for k in 0..dots {
            let Some(phi) = spin_angle(&path, (f as f64 / frames as f64 - k as f64 * lag) / life) else {
                continue;
            };
            let bearing = (180.0 - trail * k as f64 + phi).to_radians();
            write!(svg, r#"<circle cx="{}" cy="{}" r="{}" fill="{color}"/>"#, n(c + ring * bearing.sin()), n(c - ring * bearing.cos()), n(dot)).unwrap();
        }
        svg.push_str("</svg>");
        ctx.render(format!("{PLY}/throbber-{:04}.png", f + 1), &svg, px, px)?;
    }

    let w = 300.0;
    let h = 32.0;
    let entry = format!(
        r##"<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}"><rect x="0.5" y="0.5" width="{}" height="{}" rx="4" fill="#ffffff" fill-opacity="0.08" stroke="#ffffff" stroke-opacity="0.14"/><rect x="1" y="{}" width="{}" height="2" rx="1" fill="#60cdff"/></svg>"##,
        n(w - 1.0),
        n(h - 1.0),
        n(h - 2.0),
        n(w - 2.0)
    );
    ctx.render(format!("{PLY}/entry.png"), &entry, w as u32, h as u32)?;
    ctx.render(format!("{PLY}/bullet.png"), r##"<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"><circle cx="5" cy="5" r="3.5" fill="#ffffff"/></svg>"##, 10, 10)?;
    ctx.render(
        format!("{PLY}/lock.png"),
        r##"<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 20 20"><path d="M6 9V6.5a4 4 0 0 1 8 0V9" fill="none" stroke="#ffffff" stroke-width="1.6"/><rect x="4" y="9" width="12" height="9" rx="2" fill="#ffffff"/></svg>"##,
        20,
        20,
    )?;
    ctx.render(
        format!("{PLY}/capslock.png"),
        r##"<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 20 20"><path d="M10 3 3 10h4v4h6v-4h4z" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linejoin="round"/><rect x="7" y="16" width="6" height="1.6" fill="#ffffff"/></svg>"##,
        20,
        20,
    )?;
    if !bgrt {
        ctx.render(format!("{PLY}/watermark.png"), &mark(ctx, 128.0), 128, 128)?;
    }

    let bg = t.color("boot.background");
    let hex = |c: Rgba| format!("0x{}", &c.hex()[1..]);
    ctx.write(
        format!("{PLY}/lumen.plymouth"),
        format!(
            r#"[Plymouth Theme]
Name=Lumen
Description=Lumen boot splash: firmware logo with spinning dots
ModuleName=two-step

[two-step]
Font=Sans 12
TitleFont=Sans 18
ImageDir=/usr/share/plymouth/themes/lumen
DialogHorizontalAlignment=.5
DialogVerticalAlignment=.62
TitleHorizontalAlignment=.5
TitleVerticalAlignment=.55
HorizontalAlignment=.5
VerticalAlignment=.75
WatermarkHorizontalAlignment=.5
WatermarkVerticalAlignment=.45
Transition=none
TransitionDuration=0.0
BackgroundStartColor={bg}
BackgroundEndColor={bg}
ProgressBarBackgroundColor=0x3a3a3a
ProgressBarForegroundColor=0x60cdff
DialogClearsFirmwareBackground=false
MessageBelowAnimation=true
UseFirmwareBackground={fw}

[boot-up]
UseEndAnimation=false

[shutdown]
UseEndAnimation=false

[reboot]
UseEndAnimation=false

[updates]
SuppressMessages=true
ProgressBarShowPercentComplete=true
UseProgressBar=true
Title=Working on updates
SubTitle=Don't turn off your computer

[system-upgrade]
SuppressMessages=true
ProgressBarShowPercentComplete=true
UseProgressBar=true
Title=Upgrading system
SubTitle=Don't turn off your computer

[firmware-upgrade]
SuppressMessages=true
ProgressBarShowPercentComplete=true
UseProgressBar=true
Title=Upgrading firmware
SubTitle=Don't turn off your computer
"#,
            bg = hex(bg),
            fw = if bgrt { "true" } else { "false" },
        ),
    )
}

/// Lumen mark: five accent petals.
fn mark(ctx: &Ctx, s: f64) -> String {
    let sh = accent_shades(&ctx.t, ctx.t.color("accent.default"));
    let c = s / 2.0;
    let mut svg = format!(
        r#"<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{0}"><defs><linearGradient id="p" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="{1}"/><stop offset="1" stop-color="{2}"/></linearGradient></defs><g transform="translate({3},{3})">"#,
        n(s),
        sh.base.hex(),
        sh.light2.hex(),
        n(c)
    );
    for i in 0..5 {
        write!(svg, r#"<ellipse cx="{0}" cy="0" rx="{0}" ry="{1}" transform="rotate({2})" fill="url(#p)" fill-opacity="0.9"/>"#, n(s * 0.22), n(s * 0.08), n(i as f64 * 72.0 - 90.0)).unwrap();
    }
    svg.push_str("</g></svg>");
    svg
}

fn limine(ctx: &Ctx) -> Result<()> {
    let t = &ctx.t;
    let bloom = super::wallpaper::bloom(ctx, Mode::Dark);
    let dimmed = bloom.replacen("</svg>", r##"<rect width="100%" height="100%" fill="#000000" fill-opacity="0.45"/></svg>"##, 1);
    ctx.render("system/limine/lumen.png", &dimmed, 2560, 1440)?;
    let ansi = t.sv("terminal.dark.ansi");
    let pal = |r: std::ops::Range<usize>| ansi[r].iter().map(|c| c.trim_start_matches('#').to_lowercase()).collect::<Vec<_>>().join(";");
    let fg = t.color("terminal.dark.foreground").hex();
    ctx.write(
        "system/limine/theme.conf",
        format!(
            "# lumen-begin (managed by `lumen install --system`)\n\
             timeout: {}\n\
             interface_branding: Lumen\n\
             interface_help_hidden: yes\n\
             wallpaper: boot():/lumen.png\n\
             wallpaper_style: stretched\n\
             backdrop: 000000\n\
             term_palette: {}\n\
             term_palette_bright: {}\n\
             term_background: 66000000\n\
             term_foreground: {}\n\
             term_background_bright: 66202020\n\
             term_foreground_bright: ffffff\n\
             term_margin: 64\n\
             term_margin_gradient: 24\n\
             # lumen-end\n",
            t.i("boot.limine_timeout"),
            pal(0..8),
            pal(8..16),
            &fg[1..],
        ),
    )
}

fn vconsole(ctx: &Ctx) -> Result<()> {
    let colors: Vec<Rgba> = ctx.t.sv("terminal.dark.ansi").iter().map(|s| Rgba::parse(s)).collect::<Result<_>>()?;
    let mut s = String::new();
    for ch in 0..3 {
        let row: Vec<String> = colors.iter().map(|c| c.u8s()[ch].to_string()).collect();
        writeln!(s, "{}", row.join(","))?;
    }
    ctx.write("system/vconsole/vtrgb", s)?;
    ctx.write(
        "system/systemd/lumen-vtrgb.service",
        "[Unit]\nDescription=Lumen console palette\nDefaultDependencies=no\nAfter=systemd-vconsole-setup.service\nBefore=getty.target\n\n[Service]\nType=oneshot\nExecStart=/usr/bin/setvtrgb /usr/share/lumen/vtrgb\n\n[Install]\nWantedBy=sysinit.target\n",
    )
}
