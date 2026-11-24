use crate::color::Rgba;
use crate::ctx::Ctx;
use crate::svg::{Border, Frame, Line, Paint, Pill, ShadowLayer, Svg, circle, polyline, rrect, shadow_tiles};
use crate::tokens::{Mode, Tokens};
use anyhow::Result;

const BG: &str = "ColorScheme-Background";
const HL: &str = "ColorScheme-Highlight";
const NEUTRAL: &str = "ColorScheme-NeutralText";
const POSITIVE: &str = "ColorScheme-PositiveText";

/// Surface opacity per variant dir: "" (no blur), "translucent", "solid", "opaque".
const VARIANTS: [(&str, Option<f64>); 4] = [("", Some(0.97)), ("translucent/", None), ("solid/", Some(1.0)), ("opaque/", Some(1.0))];

pub fn generate(ctx: &Ctx) -> Result<()> {
    std::thread::scope(|scope| {
        let jobs: Vec<_> = Mode::ALL.into_iter().map(|m| scope.spawn(move || theme(ctx, m))).collect();
        jobs.into_iter().try_for_each(|j| j.join().expect("plasma style thread"))
    })
}

fn theme(ctx: &Ctx, m: Mode) -> Result<()> {
    let id = format!("lumen-{}", m.name());
    let files = build(&ctx.t, m);
    let mut hash: u64 = 0xcbf29ce484222325;
    for (path, svg) in &files {
        for b in path.bytes().chain(svg.bytes()) {
            hash = (hash ^ b as u64).wrapping_mul(0x100000001b3);
        }
        ctx.write(format!("share/plasma/desktoptheme/{id}/{path}"), svg)?;
    }
    ctx.write(format!("share/plasma/desktoptheme/{id}/metadata.json"), metadata(ctx, m, &id, hash))?;
    ctx.write(
        format!("share/plasma/desktoptheme/{id}/plasmarc"),
        "[Settings]\nFallbackTheme=default\n\n[BlurBehindEffect]\nenabled=true\n\n[ContrastEffect]\nenabled=false\n\n[AdaptiveTransparency]\nenabled=true\n",
    )
}

fn metadata(ctx: &Ctx, m: Mode, id: &str, hash: u64) -> String {
    format!(
        r#"{{
    "KPackageStructure": "Plasma/Theme",
    "KPlugin": {{
        "Authors": [{{ "Name": "{author}" }}],
        "Category": "",
        "Description": "Lumen {title}: Fluent-inspired Plasma style",
        "EnabledByDefault": true,
        "Id": "{id}",
        "License": "{license}",
        "Name": "Lumen {title}",
        "Version": "{ver}.{hash:08x}",
        "Website": "{web}"
    }},
    "X-Plasma-API": "5.0"
}}
"#,
        author = ctx.t.s("meta.author"),
        title = m.title(),
        license = ctx.t.s("meta.license"),
        ver = ctx.version(),
        hash = hash as u32,
        web = ctx.t.s("meta.website"),
    )
}

struct P<'a> {
    t: &'a Tokens,
    m: Mode,
}

impl P<'_> {
    fn c(&self, k: &str) -> Paint {
        Paint::Color(self.t.c(self.m, k))
    }
    fn rc(&self, k: &str) -> Rgba {
        self.t.c(self.m, k)
    }
    fn dark(&self) -> bool {
        self.m == Mode::Dark
    }
    fn r(&self, k: &str) -> f64 {
        self.t.f(&format!("radius.{k}"))
    }
    fn alpha(&self, k: &str) -> f64 {
        self.t.fm(&format!("surface.{k}"), self.m)
    }
    fn layers(&self, k: &str) -> Vec<ShadowLayer> {
        let col = if self.dark() { 3 } else { 4 };
        self.t.fvv(&format!("shadow.{k}")).iter().map(|r| ShadowLayer { x: r[0], y: r[1], blur: r[2], alpha: r[col] }).collect()
    }
    /// Fluent elevation border: brighter top in dark, darker bottom in light.
    fn elevation(&self) -> Border {
        let (d, s) = (self.c("control_stroke"), self.c("control_stroke_secondary"));
        if self.dark() { Border::elevation(s, d, d) } else { Border::elevation(d, d, s) }
    }
    fn flyout_border(&self) -> (Border, Option<Line>) {
        let outer = Border::uniform(self.c("surface_stroke_flyout"), 1.0);
        let inner = if self.dark() { Line::new(Paint::Color(Rgba::WHITE.alpha(0.07)), 1.0) } else { None };
        (outer, inner)
    }
}

fn build(t: &Tokens, m: Mode) -> Vec<(String, String)> {
    let p = P { t, m };
    let mut files = Vec::new();
    let overlay = p.r("overlay");

    for (dir, op) in VARIANTS {
        let a = |k: &str| op.unwrap_or_else(|| p.alpha(k));
        // Windows flyouts and toasts keep 12 px between their edge and content (notification close and settings buttons).
        files.push((format!("{dir}dialogs/background.svg"), surface(&p, overlay, a("dialog_alpha"), "flyout", [t.f("size.flyout_pad"); 4])));
        files.push((format!("{dir}widgets/tooltip.svg"), surface(&p, p.r("tooltip"), a("tooltip_alpha"), "tooltip", tooltip_pad(t))));
        files.push((format!("{dir}widgets/panel-background.svg"), panel(&p, a("panel_alpha"))));
        if dir != "opaque/" {
            files.push((format!("{dir}widgets/background.svg"), surface(&p, overlay, a("dialog_alpha"), "flyout", [12.0; 4])));
        }
    }
    files.push(("widgets/translucentbackground.svg".into(), surface(&p, overlay, p.alpha("dialog_alpha") * 0.7, "flyout", [12.0; 4])));
    files.push(("widgets/tasks.svg".into(), tasks(&p)));
    files.push(("widgets/button.svg".into(), button(&p)));
    files.push(("widgets/lineedit.svg".into(), lineedit(&p)));
    files.push(("widgets/scrollbar.svg".into(), scrollbar(&p)));
    files.push(("widgets/slider.svg".into(), slider(&p)));
    files.push(("widgets/switch.svg".into(), switch(&p)));
    files.push(("widgets/radiobutton.svg".into(), radiobutton(&p)));
    files.push(("widgets/checkmarks.svg".into(), checkmarks(&p)));
    files.push(("widgets/viewitem.svg".into(), viewitem(&p)));
    files.push(("widgets/listitem.svg".into(), listitem(&p)));
    files.push(("widgets/menubaritem.svg".into(), menubaritem(&p)));
    files.push(("widgets/plasmoidheading.svg".into(), heading(&p)));
    files.push(("widgets/frame.svg".into(), frame(&p)));
    files.push(("widgets/tabbar.svg".into(), tabbar(&p)));
    files.push(("widgets/bar_meter_horizontal.svg".into(), bar_meter(&p)));
    files
}

fn tooltip_pad(t: &Tokens) -> [f64; 4] {
    let v = t.fv("size.tooltip_pad");
    [v[0], v[1], v[2], v[3]]
}

fn surface(p: &P, radius: f64, alpha: f64, shadow: &str, margins: [f64; 4]) -> String {
    let mut s = Svg::new();
    let (border, inner) = p.flyout_border();
    s.frame(&Frame::new("", radius, Paint::Class(BG, alpha)).corner(radius + 1.0).border(border).inner(inner).mask().margins(margins));
    s.shadow(&shadow_tiles(&p.layers(shadow), radius, Rgba::BLACK));
    s.finish()
}

fn panel(p: &P, alpha: f64) -> String {
    let mut s = Svg::new();
    let line = Paint::Color(if p.dark() { Rgba::WHITE.alpha(0.08) } else { Rgba::BLACK.alpha(0.08) });
    let r = p.r("overlay");
    s.frame(&Frame::new("", r, Paint::Class(BG, alpha)).corner(r + 1.0).border(Border::uniform(line, 1.0)).mask().margins([2.0; 4]));
    s.shadow(&shadow_tiles(&p.layers("tooltip"), r, Rgba::BLACK));
    s.finish()
}

fn tasks(p: &P) -> String {
    let t = p.t;
    let mut s = Svg::new();
    let w = t.f("size.taskbar_button");
    let r = p.r("taskbar_button");
    let ph = t.f("size.taskbar_indicator_h");
    let active = t.f("size.taskbar_indicator_active");
    let inactive = t.f("size.taskbar_indicator_inactive");
    let pad = 2.0;
    let pill = |paint: Paint| Pill { paint, thickness: ph, offset: 1.0 };
    let cw = |pill_w: f64| ((w - pill_w) / 2.0).max(r + pad);
    let icon = t.f("size.taskbar_icon");
    let v = ((t.f("size.taskbar_h") - icon) / 2.0).max(4.0);
    let margins = [(w - icon) / 2.0, v, (w - icon) / 2.0, v];
    let pad_v = ((t.f("size.taskbar_h") - t.f("size.taskbar_plate")) / 2.0).max(pad);
    let base = |prefix: &str, fill: Paint| Frame::new(prefix, r, fill).corner(pad_v + r).inset(pad).inset_v(pad_v).margins(margins);

    s.frame(&base("normal", Paint::None).corner_w(cw(inactive)).bottom_pill(pill(p.c("text_tertiary"))));
    s.frame(&base("minimized", Paint::None).corner_w(cw(inactive)).bottom_pill(pill(p.c("text_disabled"))));
    s.frame(&base("hover", p.c("control_fill_hover")).border(p.elevation()).corner_w(cw(inactive)));
    // Hovered states keep their indicator; pinned apps that aren't running ask for "launcher-hover".
    s.frame(&base("launcher-hover", p.c("control_fill_hover")).border(p.elevation()).corner_w(cw(inactive)));
    s.frame(&base("normal-hover", p.c("control_fill_hover")).border(p.elevation()).corner_w(cw(inactive)).bottom_pill(pill(p.c("text_tertiary"))));
    s.frame(&base("minimized-hover", p.c("control_fill_hover")).border(p.elevation()).corner_w(cw(inactive)).bottom_pill(pill(p.c("text_disabled"))));
    for prefix in ["focus", "focus-hover"] {
        s.frame(&base(prefix, p.c("control_fill_hover")).corner_w(cw(active)).border(p.elevation()).bottom_pill(pill(Paint::Class(HL, 1.0))));
    }
    for prefix in ["attention", "attention-hover"] {
        s.frame(&base(prefix, Paint::Class(NEUTRAL, 0.18)).corner_w(cw(active)).bottom_pill(pill(Paint::Class(NEUTRAL, 1.0))));
    }
    s.frame(&base("progress", Paint::Class(POSITIVE, 0.22)).corner_w(cw(active)));
    s.finish()
}

fn button(p: &P) -> String {
    let mut s = Svg::new();
    let r = p.r("control");
    let m = [10.0, 5.0, 10.0, 5.0];
    let f = |prefix: &str, fill: Paint| Frame::new(prefix, r, fill).corner(r + 1.0).margins(m);
    s.frame(&f("normal", p.c("control_fill")).border(p.elevation()).mask());
    // Plasma draws hover/focus frames outside the control, grown by their own margins: a 1 px margin with a
    // 1 px transparent inset lines them up with the control instead of a box around it.
    let outer = |prefix: &str, fill: Paint| Frame::new(prefix, r, fill).corner(r + 2.0).inset(1.0).margins([1.0; 4]);
    s.frame(&outer("hover", p.c("control_fill_hover")).border(p.elevation()));
    s.frame(&f("pressed", p.c("control_fill_pressed")).border(Border::uniform(p.c("control_stroke"), 1.0)));
    s.frame(&outer("focus", Paint::None).border(Border::uniform(Paint::Class(HL, 1.0), 2.0)));
    s.frame(&outer("toolbutton-hover", p.c("subtle_fill_hover")));
    s.frame(&f("toolbutton-pressed", p.c("subtle_fill_pressed")));
    s.frame(&outer("toolbutton-focus", Paint::None).border(Border::uniform(Paint::Class(HL, 1.0), 2.0)));
    s.finish()
}

fn lineedit(p: &P) -> String {
    let mut s = Svg::new();
    let r = p.r("control");
    let m = [10.0, 6.0, 10.0, 6.0];
    let stroke = p.c("control_stroke");
    let rest = Border { top: Line::new(stroke, 1.0), side: Line::new(stroke, 1.0), bottom: Line::new(p.c("control_strong_stroke"), 1.0) };
    let focus = Border { bottom: Line::new(Paint::Class(HL, 1.0), 2.0), ..rest };
    let f = |prefix: &str, fill: Paint, b: Border| Frame::new(prefix, r, fill).corner(r + 1.0).border(b).margins(m);
    let outer = |prefix: &str, fill: Paint, b: Border| Frame::new(prefix, r, fill).corner(r + 2.0).inset(1.0).border(b).margins([1.0; 4]);
    s.frame(&f("base", p.c("control_fill"), rest));
    s.frame(&outer("hover", p.c("control_fill_hover"), rest));
    s.frame(&outer("focus", p.c("control_fill_input_active"), focus));
    s.flag("hint-focus-over-base");
    s.finish()
}

fn scrollbar(p: &P) -> String {
    let mut s = Svg::new();
    let size = 10.0;
    s.hint("hint-scrollbar-size", size, size);
    for o in ["vertical", "horizontal"] {
        s.frame(&Frame::new(&format!("background-{o}"), 4.0, Paint::None).corner(4.0).insets([0.0; 4]));
    }
    let ins = [2.0, 2.0, 2.0, 2.0];
    s.frame(&Frame::new("slider", 3.0, p.c("control_strong_fill")).corner(4.0).insets(ins));
    s.frame(&Frame::new("mouseover-slider", 3.0, p.c("text_secondary")).corner(4.0).insets(ins));
    s.finish()
}

fn slider(p: &P) -> String {
    let t = p.t;
    let mut s = Svg::new();
    let track = t.f("size.slider_track");
    let d = t.f("size.slider_thumb");
    let (inner, inner_h) = (t.f("size.slider_thumb_inner") / 2.0, t.f("size.slider_thumb_inner_hover") / 2.0);
    s.frame(&Frame::new("groove", track / 2.0, p.c("control_strong_fill")).corner(track / 2.0));
    s.frame(&Frame::new("groove-highlight", track / 2.0, Paint::Class(HL, 1.0)).corner(track / 2.0));
    s.hint("hint-bar-size", track, track);
    s.hint("hint-handle-size", d, d);
    let ring = Line::new(p.c("control_stroke_secondary"), 1.0);
    let solid = p.c("control_solid_fill");
    for o in ["horizontal", "vertical"] {
        let thumb = |rr: f64| move |x: f64, y: f64| circle(x + d / 2.0, y + d / 2.0, d / 2.0 - 0.5, solid, ring) + &circle(x + d / 2.0, y + d / 2.0, rr, Paint::Class(HL, 1.0), None);
        s.element(&format!("{o}-slider-handle"), d, d, thumb(inner));
        s.element(&format!("{o}-slider-hover"), d, d, thumb(inner_h));
        s.element(&format!("{o}-slider-focus"), d, d, thumb(inner_h));
        s.element(&format!("{o}-slider-shadow"), d, d, |_, _| String::new());
    }
    s.finish()
}

fn switch(p: &P) -> String {
    let t = p.t;
    let mut s = Svg::new();
    let (w, h) = (t.f("size.switch_w"), t.f("size.switch_h"));
    let r = h / 2.0;
    s.frame(&Frame::new("inactive", r, p.c("control_alt_fill")).corner(r).border(Border::uniform(p.c("control_strong_stroke"), 1.0)));
    s.frame(&Frame::new("active", r, Paint::Class(HL, 1.0)).corner(r));
    s.hint("hint-bar-size", w, h);
    // WinUI knob: 12 px, 14 px under the pointer; secondary text color when off, on-accent text color when on.
    // Plasma has no checked variant of the hover and pressed knobs, so those stay readable on either track.
    let (knob, knob_h) = (t.f("size.switch_knob") / 2.0, t.f("size.switch_knob_hover") / 2.0);
    let off = p.c("text_secondary");
    let on = p.c("text_on_accent");
    let (hot, hot_ring) = if p.dark() { (off, None) } else { (Paint::Color(Rgba::WHITE), Line::new(Paint::Color(Rgba::BLACK.alpha(0.18)), 1.0)) };
    let k = |rr: f64, fill: Paint, ring: Option<Line>| move |x: f64, y: f64| circle(x + h / 2.0, y + h / 2.0, rr, fill, ring);
    s.element("handle", h, h, k(knob, off, None));
    s.element("handle-active", h, h, k(knob, on, None));
    s.element("handle-hover", h, h, k(knob_h, hot, hot_ring));
    s.element("handle-pressed", h, h, k(knob_h, hot, hot_ring));
    s.element("handle-focus", h, h, move |x, y| circle(x + h / 2.0, y + h / 2.0, h / 2.0 - 1.0, Paint::None, Line::new(Paint::Class(HL, 1.0), 2.0)));
    s.element("handle-shadow", h, h, |_, _| String::new());
    s.finish()
}

fn radiobutton(p: &P) -> String {
    let t = p.t;
    let mut s = Svg::new();
    let d = t.f("size.radio");
    let c = d / 2.0;
    let strong = Line::new(p.c("control_strong_stroke"), 1.0);
    s.element("normal", d, d, |x, y| circle(x + c, y + c, c - 0.5, p.c("control_alt_fill"), strong));
    s.element("hover", d, d, |x, y| circle(x + c, y + c, c - 0.5, p.c("control_alt_fill_hover"), strong));
    s.element("focus", d, d, |x, y| circle(x + c, y + c, c - 1.0, Paint::None, Line::new(Paint::Class(HL, 1.0), 2.0)));
    s.element("checked", d, d, |x, y| circle(x + c, y + c, c, Paint::Class(HL, 1.0), None));
    let dot = t.f("size.radio_glyph") / 2.0;
    s.element("symbol", d, d, |x, y| circle(x + c, y + c, dot, p.c("text_on_accent"), None));
    s.element("shadow", d, d, |_, _| String::new());
    s.finish()
}

/// Plasma check boxes draw this over a button frame (opacity 0.5 when partially checked): Windows' filled accent box.
fn checkmarks(p: &P) -> String {
    let mut s = Svg::new();
    let d = 16.0;
    let r = p.r("control") * d / p.t.f("size.checkbox");
    s.element("checkbox", d, d, |x, y| {
        rrect(x, y, d, d, r, Paint::Class(HL, 1.0), None)
            + &polyline(&[(x + 4.5, y + 8.5), (x + 7.0, y + 11.0), (x + 11.5, y + 5.5)], Line::new(p.c("text_on_accent"), 1.5).unwrap())
    });
    s.finish()
}

fn viewitem(p: &P) -> String {
    let t = p.t;
    let mut s = Svg::new();
    let r = p.r("control");
    let pill_h = t.f("size.list_pill_h");
    let c = ((t.f("size.list_item_h") - pill_h) / 2.0).max(r);
    let pill = Pill { paint: Paint::Class(HL, 1.0), thickness: t.f("size.list_pill_w"), offset: 0.0 };
    let m = [8.0, 4.0, 8.0, 4.0];
    let f = |prefix: &str, fill: Paint| Frame::new(prefix, r, fill).corner(c).corner_w(r + 1.0).margins(m);
    s.frame(&f("normal", Paint::None));
    s.frame(&f("hover", p.c("subtle_fill_hover")));
    s.frame(&f("selected", p.c("subtle_fill_hover")).left_pill(pill));
    s.frame(&f("selected+hover", p.c("control_fill_hover")).left_pill(pill));
    s.finish()
}

fn listitem(p: &P) -> String {
    let mut s = Svg::new();
    let r = p.r("control");
    let m = [8.0, 4.0, 8.0, 4.0];
    let f = |prefix: &str, fill: Paint| Frame::new(prefix, r, fill).corner(r + 1.0).margins(m);
    s.frame(&f("normal", Paint::None));
    s.frame(&f("hover", p.c("subtle_fill_hover")));
    s.frame(&f("pressed", p.c("subtle_fill_pressed")));
    s.frame(&f("section", Paint::None).border(Border { bottom: Line::new(p.c("divider"), 1.0), ..Border::default() }));
    let div = p.rc("divider");
    s.element("separator", 10.0, 1.0, |x, y| crate::svg::rrect(x, y, 10.0, 1.0, 0.0, Paint::Color(div), None));
    s.finish()
}

fn menubaritem(p: &P) -> String {
    let mut s = Svg::new();
    let r = p.r("control");
    let m = [8.0, 4.0, 8.0, 4.0];
    let f = |prefix: &str, fill: Paint| Frame::new(prefix, r, fill).corner(r + 1.0).margins(m);
    s.frame(&f("normal", Paint::None));
    s.frame(&f("hover", p.c("subtle_fill_hover")));
    s.frame(&f("pressed", p.c("subtle_fill_pressed")));
    s.finish()
}

fn heading(p: &P) -> String {
    let mut s = Svg::new();
    let div = Line::new(p.c("divider"), 1.0);
    let r = p.r("overlay") - 2.0;
    // Header without a divider (notification popups, applet popups); the footer keeps its line.
    s.frame(&Frame::new("header", 0.0, Paint::None).corner(2.0).margins([8.0; 4]));
    s.frame(&Frame::new("footer", r, p.c("layer_on_acrylic")).corner(r).border(Border { top: div, ..Border::default() }).margins([8.0; 4]));
    s.finish()
}

fn frame(p: &P) -> String {
    let mut s = Svg::new();
    let r = p.r("control");
    let m = [4.0; 4];
    let stroke = Border::uniform(p.c("card_stroke"), 1.0);
    s.frame(&Frame::new("plain", r, Paint::None).corner(r + 1.0).border(stroke).margins(m));
    s.frame(&Frame::new("raised", r, p.c("card_bg")).corner(r + 1.0).border(stroke).margins(m));
    s.frame(&Frame::new("sunken", r, p.c("control_alt_fill")).corner(r + 1.0).border(Border::uniform(p.c("control_stroke"), 1.0)).margins(m));
    s.finish()
}

fn tabbar(p: &P) -> String {
    let mut s = Svg::new();
    let pill = Pill { paint: Paint::Class(HL, 1.0), thickness: 3.0, offset: 0.0 };
    s.frame(&Frame::new("north-active-tab", 4.0, p.c("subtle_fill_hover")).corner(6.0).corner_w(12.0).bottom_pill(pill).margins([8.0, 6.0, 8.0, 6.0]));
    s.frame(&Frame::new("south-active-tab", 4.0, p.c("subtle_fill_hover")).corner(6.0).margins([8.0, 6.0, 8.0, 6.0]));
    s.finish()
}

fn bar_meter(p: &P) -> String {
    let t = p.t;
    let mut s = Svg::new();
    let h = t.f("size.progress_h");
    s.frame(&Frame::new("bar-inactive", h / 2.0, p.c("control_strong_stroke_disabled")).corner(h / 2.0));
    s.frame(&Frame::new("bar-active", h / 2.0, Paint::Class(HL, 1.0)).corner(h / 2.0));
    s.hint("hint-bar-size", h, h);
    s.finish()
}
