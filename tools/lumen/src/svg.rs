//! Plasma (KSvg) SVG builder: 9-slice frames, hint rects, single elements, raster shadow tiles.
//! Every element group carries an invisible rect spanning its cell because KSvg sizes slices
//! from element bounds.

use crate::color::Rgba;
use resvg::tiny_skia;
use std::fmt::Write;

const STYLE: &str = r#"<style type="text/css" id="current-color-scheme">
.ColorScheme-Text{color:#232629;}.ColorScheme-Background{color:#eff0f1;}.ColorScheme-Highlight{color:#3daee9;}
.ColorScheme-HighlightedText{color:#ffffff;}.ColorScheme-ViewText{color:#232629;}.ColorScheme-ViewBackground{color:#fcfcfc;}
.ColorScheme-ButtonText{color:#232629;}.ColorScheme-ButtonBackground{color:#eff0f1;}.ColorScheme-ButtonFocus{color:#3daee9;}
.ColorScheme-ButtonHover{color:#3daee9;}.ColorScheme-NegativeText{color:#da4453;}.ColorScheme-NeutralText{color:#f67400;}
.ColorScheme-PositiveText{color:#27ae60;}.ColorScheme-ComplementaryText{color:#fcfcfc;}.ColorScheme-ComplementaryBackground{color:#2a2e32;}
</style>"#;

#[derive(Clone, Copy, Debug)]
pub enum Paint {
    None,
    /// KSvg color-scheme class + opacity.
    Class(&'static str, f64),
    Color(Rgba),
}

impl Paint {
    pub fn is_none(&self) -> bool {
        matches!(self, Paint::None) || matches!(self, Paint::Color(c) if c.a <= 0.0)
    }

    fn attr(&self, kind: &str) -> String {
        match *self {
            Paint::None => format!("{kind}=\"none\""),
            Paint::Class(class, op) => format!("class=\"{class}\" {kind}=\"currentColor\" {kind}-opacity=\"{}\"", n(op)),
            Paint::Color(c) => {
                if c.a >= 1.0 {
                    format!("{kind}=\"{}\"", c.hex())
                } else {
                    format!("{kind}=\"{}\" {kind}-opacity=\"{}\"", c.hex(), n(c.a))
                }
            }
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct Line {
    pub paint: Paint,
    pub w: f64,
}

impl Line {
    pub fn new(paint: Paint, w: f64) -> Option<Line> {
        (!paint.is_none() && w > 0.0).then_some(Line { paint, w })
    }
}

/// Per-side border (Fluent elevation borders differ top vs bottom).
#[derive(Clone, Copy, Debug, Default)]
pub struct Border {
    pub top: Option<Line>,
    pub side: Option<Line>,
    pub bottom: Option<Line>,
}

impl Border {
    pub fn uniform(paint: Paint, w: f64) -> Self {
        let l = Line::new(paint, w);
        Self { top: l, side: l, bottom: l }
    }

    pub fn elevation(top: Paint, side: Paint, bottom: Paint) -> Self {
        Self { top: Line::new(top, 1.0), side: Line::new(side, 1.0), bottom: Line::new(bottom, 1.0) }
    }
}

/// Indicator drawn along the stretched bottom edge (taskbar pill, list pill uses `left`).
#[derive(Clone, Copy, Debug)]
pub struct Pill {
    pub paint: Paint,
    pub thickness: f64,
    pub offset: f64,
}

#[derive(Clone, Debug)]
pub struct Frame {
    pub prefix: String,
    pub radius: f64,
    /// Corner cell size (height; width too unless `corner_w`).
    pub corner: f64,
    pub corner_w: Option<f64>,
    pub edge: f64,
    /// Transparent padding between cell bounds and the painted shape.
    pub inset: f64,
    /// Top and bottom inset when it differs from the side one.
    pub inset_v: Option<f64>,
    pub fill: Paint,
    pub border: Border,
    /// Second ring one border-width inside (Fluent flyout highlight).
    pub inner: Option<Line>,
    pub bottom_pill: Option<Pill>,
    pub left_pill: Option<Pill>,
    pub mask: bool,
    /// Content margins l, t, r, b.
    pub margins: Option<[f64; 4]>,
    pub insets: Option<[f64; 4]>,
}

impl Frame {
    pub fn new(prefix: &str, radius: f64, fill: Paint) -> Self {
        Self {
            prefix: prefix.to_owned(),
            radius,
            corner: radius.max(2.0),
            corner_w: None,
            edge: 10.0,
            inset: 0.0,
            inset_v: None,
            fill,
            border: Border::default(),
            inner: None,
            bottom_pill: None,
            left_pill: None,
            mask: false,
            margins: None,
            insets: None,
        }
    }
    pub fn corner(mut self, c: f64) -> Self {
        self.corner = c;
        self
    }
    pub fn corner_w(mut self, w: f64) -> Self {
        self.corner_w = Some(w);
        self
    }
    pub fn inset(mut self, i: f64) -> Self {
        self.inset = i;
        self
    }
    pub fn inset_v(mut self, i: f64) -> Self {
        self.inset_v = Some(i);
        self
    }
    pub fn border(mut self, b: Border) -> Self {
        self.border = b;
        self
    }
    pub fn inner(mut self, l: Option<Line>) -> Self {
        self.inner = l;
        self
    }
    pub fn bottom_pill(mut self, p: Pill) -> Self {
        self.bottom_pill = Some(p);
        self
    }
    pub fn left_pill(mut self, p: Pill) -> Self {
        self.left_pill = Some(p);
        self
    }
    pub fn mask(mut self) -> Self {
        self.mask = true;
        self
    }
    pub fn margins(mut self, m: [f64; 4]) -> Self {
        self.margins = Some(m);
        self
    }
    pub fn insets(mut self, m: [f64; 4]) -> Self {
        self.insets = Some(m);
        self
    }
}

pub struct Svg {
    body: String,
    x: f64,
    y: f64,
    row_h: f64,
    w: f64,
    stretch_hint: bool,
}

const PAGE_W: f64 = 1200.0;
const GAP: f64 = 8.0;

impl Default for Svg {
    fn default() -> Self {
        Self::new()
    }
}

impl Svg {
    pub fn new() -> Self {
        Self { body: String::with_capacity(32 * 1024), x: GAP, y: GAP, row_h: 0.0, w: 0.0, stretch_hint: false }
    }

    fn alloc(&mut self, w: f64, h: f64) -> (f64, f64) {
        if self.x + w + GAP > PAGE_W && self.x > GAP {
            self.x = GAP;
            self.y += self.row_h + GAP;
            self.row_h = 0.0;
        }
        let pos = (self.x, self.y);
        self.x += w + GAP;
        self.row_h = self.row_h.max(h);
        self.w = self.w.max(self.x);
        pos
    }

    fn id(prefix: &str, name: &str) -> String {
        if prefix.is_empty() { name.to_owned() } else { format!("{prefix}-{name}") }
    }

    /// Hint rect: only its size matters.
    pub fn hint(&mut self, id: &str, w: f64, h: f64) {
        let (x, y) = self.alloc(w.max(1.0), h.max(1.0));
        writeln!(self.body, r#"<rect id="{id}" x="{}" y="{}" width="{}" height="{}" fill="none"/>"#, n(x), n(y), n(w), n(h)).unwrap();
    }

    pub fn flag(&mut self, id: &str) {
        self.hint(id, 1.0, 1.0);
    }

    /// Single element of size w×h; `draw(x, y)` returns inner markup in absolute coords.
    pub fn element(&mut self, id: &str, w: f64, h: f64, draw: impl FnOnce(f64, f64) -> String) {
        let (x, y) = self.alloc(w, h);
        let inner = draw(x, y);
        writeln!(self.body, r#"<g id="{id}">{}{inner}</g>"#, bounds(x, y, w, h)).unwrap();
    }

    pub fn image(&mut self, id: &str, w: f64, h: f64, png: &[u8]) {
        if w < 1.0 || h < 1.0 {
            return;
        }
        let (x, y) = self.alloc(w, h);
        writeln!(
            self.body,
            r#"<g id="{id}">{}<image x="{}" y="{}" width="{}" height="{}" preserveAspectRatio="none" xlink:href="data:image/png;base64,{}"/></g>"#,
            bounds(x, y, w, h),
            n(x),
            n(y),
            n(w),
            n(h),
            base64(png)
        )
        .unwrap();
    }

    pub fn frame(&mut self, f: &Frame) {
        if !self.stretch_hint {
            self.stretch_hint = true;
            self.flag("hint-stretch-borders");
        }
        self.frame_cells(f, &f.prefix, false);
        if f.mask {
            let mp = if f.prefix.is_empty() { "mask".to_owned() } else { format!("mask-{}", f.prefix) };
            self.frame_cells(f, &mp, true);
        }
        let p = &f.prefix;
        if let Some([l, t, r, b]) = f.margins {
            self.hint(&Self::id(p, "hint-left-margin"), l, 4.0);
            self.hint(&Self::id(p, "hint-top-margin"), 4.0, t);
            self.hint(&Self::id(p, "hint-right-margin"), r, 4.0);
            self.hint(&Self::id(p, "hint-bottom-margin"), 4.0, b);
        }
        if let Some([l, t, r, b]) = f.insets {
            self.hint(&Self::id(p, "hint-left-inset"), l, 4.0);
            self.hint(&Self::id(p, "hint-top-inset"), 4.0, t);
            self.hint(&Self::id(p, "hint-right-inset"), r, 4.0);
            self.hint(&Self::id(p, "hint-bottom-inset"), 4.0, b);
        }
    }

    fn frame_cells(&mut self, f: &Frame, prefix: &str, mask: bool) {
        let c = f.corner;
        let cw = f.corner_w.unwrap_or(c);
        let e = f.edge;
        let bw = 2.0 * cw + e;
        let bh = 2.0 * c + e;
        let (x0, y0) = self.alloc(bw, bh);
        let xs = [x0, x0 + cw, x0 + cw + e, x0 + bw];
        let ys = [y0, y0 + c, y0 + c + e, y0 + bh];
        let iv = f.inset_v.unwrap_or(f.inset);
        let (ax0, ay0, ax1, ay1) = (x0 + f.inset, y0 + iv, x0 + bw - f.inset, y0 + bh - iv);
        let r = f.radius;
        let fill = if mask { Paint::Color(Rgba::BLACK) } else { f.fill };
        let border = if mask { Border::default() } else { f.border };
        let inner = if mask { None } else { f.inner };

        // corners
        for (name, ax, ay, bx, by) in [
            ("topleft", ax0, ay0, xs[1], ys[1]),
            ("topright", ax1, ay0, xs[2], ys[1]),
            ("bottomleft", ax0, ay1, xs[1], ys[2]),
            ("bottomright", ax1, ay1, xs[2], ys[2]),
        ] {
            let (cx0, cx1) = if ax < bx { (xs[0], xs[1]) } else { (xs[2], xs[3]) };
            let (cy0, cy1) = if ay < by { (ys[0], ys[1]) } else { (ys[2], ys[3]) };
            let dx = (bx - ax).signum();
            let dy = (by - ay).signum();
            let sweep = if dx * dy > 0.0 { 1 } else { 0 };
            let mut g = bounds(cx0, cy0, cx1 - cx0, cy1 - cy0);
            if !fill.is_none() {
                let d = format!(
                    "M{},{} A{},{} 0 0 {sweep} {},{} L{},{} L{},{} L{},{} Z",
                    n(ax),
                    n(ay + dy * r),
                    n(r),
                    n(r),
                    n(ax + dx * r),
                    n(ay),
                    n(bx),
                    n(ay),
                    n(bx),
                    n(by),
                    n(ax),
                    n(by)
                );
                let _ = write!(g, r#"<path d="{d}" {}/>"#, fill.attr("fill"));
            }
            let outer = if ay < by { border.top } else { border.bottom };
            let ow = outer.map_or(0.0, |l| l.w);
            let rings = [outer.map(|l| (l, l.w / 2.0)), inner.map(|l| (l, ow + l.w / 2.0))];
            for (line, h) in rings.into_iter().flatten() {
                // Filled ring, not a stroke: stroke half-widths would grow the element bounds past the cell.
                let (o, i) = (h - line.w / 2.0, h + line.w / 2.0);
                let (ro, ri) = ((r - o).max(0.0), (r - i).max(0.0));
                let d = format!(
                    "M{},{} L{},{} A{},{} 0 0 {sweep} {},{} L{},{} L{},{} L{},{} A{},{} 0 0 {} {},{} L{},{} Z",
                    n(ax + dx * o),
                    n(by),
                    n(ax + dx * o),
                    n(ay + dy * r),
                    n(ro),
                    n(ro),
                    n(ax + dx * r),
                    n(ay + dy * o),
                    n(bx),
                    n(ay + dy * o),
                    n(bx),
                    n(ay + dy * i),
                    n(ax + dx * r),
                    n(ay + dy * i),
                    n(ri),
                    n(ri),
                    1 - sweep,
                    n(ax + dx * i),
                    n(ay + dy * r),
                    n(ax + dx * i),
                    n(by)
                );
                let _ = write!(g, r#"<path d="{d}" {}/>"#, line.paint.attr("fill"));
            }
            self.group(&Self::id(prefix, name), &g);
        }

        // edges
        let edge = |svg: &mut Svg, name: &str, rx: f64, ry: f64, rw: f64, rh: f64, cell: [f64; 4], lines: Vec<String>| {
            let mut g = bounds(cell[0], cell[1], cell[2], cell[3]);
            if !fill.is_none() && rw > 0.0 && rh > 0.0 {
                let _ = write!(g, r#"<rect x="{}" y="{}" width="{}" height="{}" {}/>"#, n(rx), n(ry), n(rw), n(rh), fill.attr("fill"));
            }
            for l in lines {
                g.push_str(&l);
            }
            svg.group(&Self::id(prefix, name), &g);
        };
        let tw = border.top.map_or(0.0, |l| l.w);
        let bw_ = border.bottom.map_or(0.0, |l| l.w);
        let sw = border.side.map_or(0.0, |l| l.w);
        let hseg = |y: f64, l: Line| seg(xs[1], y, xs[2], y, l);
        let vseg = |x: f64, l: Line| seg(x, ys[1], x, ys[2], l);

        let mut top_lines = Vec::new();
        if let Some(l) = border.top {
            top_lines.push(hseg(ay0 + l.w / 2.0, l));
        }
        if let Some(l) = inner {
            top_lines.push(hseg(ay0 + tw + l.w / 2.0, l));
        }
        edge(self, "top", xs[1], ay0, e, ys[1] - ay0, [xs[1], ys[0], e, c], top_lines);

        let mut bottom_lines = Vec::new();
        if let Some(l) = border.bottom {
            bottom_lines.push(hseg(ay1 - l.w / 2.0, l));
        }
        if let Some(l) = inner {
            bottom_lines.push(hseg(ay1 - bw_ - l.w / 2.0, l));
        }
        if !mask && let Some(p) = f.bottom_pill {
            let y = ay1 - p.offset - p.thickness;
            bottom_lines.push(format!(
                r#"<rect x="{}" y="{}" width="{}" height="{}" rx="{}" {}/>"#,
                n(xs[1]),
                n(y),
                n(e),
                n(p.thickness),
                n(p.thickness / 2.0),
                p.paint.attr("fill")
            ));
        }
        edge(self, "bottom", xs[1], ys[2], e, ay1 - ys[2], [xs[1], ys[2], e, c], bottom_lines);

        let mut left_lines = Vec::new();
        if let Some(l) = border.side {
            left_lines.push(vseg(ax0 + l.w / 2.0, l));
        }
        if let Some(l) = inner {
            left_lines.push(vseg(ax0 + sw + l.w / 2.0, l));
        }
        if !mask && let Some(p) = f.left_pill {
            left_lines.push(format!(
                r#"<rect x="{}" y="{}" width="{}" height="{}" rx="{}" {}/>"#,
                n(ax0 + p.offset),
                n(ys[1]),
                n(p.thickness),
                n(e),
                n(p.thickness / 2.0),
                p.paint.attr("fill")
            ));
        }
        edge(self, "left", ax0, ys[1], xs[1] - ax0, e, [xs[0], ys[1], cw, e], left_lines);

        let mut right_lines = Vec::new();
        if let Some(l) = border.side {
            right_lines.push(vseg(ax1 - l.w / 2.0, l));
        }
        if let Some(l) = inner {
            right_lines.push(vseg(ax1 - sw - l.w / 2.0, l));
        }
        edge(self, "right", xs[2], ys[1], ax1 - xs[2], e, [xs[2], ys[1], cw, e], right_lines);

        edge(self, "center", xs[1], ys[1], e, e, [xs[1], ys[1], e, e], Vec::new());
    }

    fn group(&mut self, id: &str, inner: &str) {
        writeln!(self.body, r#"<g id="{id}">{inner}</g>"#).unwrap();
    }

    pub fn finish(self) -> String {
        let h = self.y + self.row_h + GAP;
        format!(
            "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\" width=\"{w}\" height=\"{h}\" viewBox=\"0 0 {w} {h}\">\n<defs>{STYLE}</defs>\n{}</svg>\n",
            self.body,
            w = n(self.w),
            h = n(h)
        )
    }
}

fn bounds(x: f64, y: f64, w: f64, h: f64) -> String {
    format!(r#"<rect x="{}" y="{}" width="{}" height="{}" fill="none"/>"#, n(x), n(y), n(w), n(h))
}

/// Axis-aligned line as a filled rect (exact bounds, so tiled/stretched frame edges have no seams).
fn seg(x0: f64, y0: f64, x1: f64, y1: f64, l: Line) -> String {
    let h = l.w / 2.0;
    let (x, y, w, ht) = if y0 == y1 { (x0.min(x1), y0 - h, (x1 - x0).abs(), l.w) } else { (x0 - h, y0.min(y1), l.w, (y1 - y0).abs()) };
    format!(r#"<rect x="{}" y="{}" width="{}" height="{}" {}/>"#, n(x), n(y), n(w), n(ht), l.paint.attr("fill"))
}

pub fn n(v: f64) -> String {
    crate::tokens::fmt_num((v * 1000.0).round() / 1000.0)
}

pub fn circle(cx: f64, cy: f64, r: f64, fill: Paint, stroke: Option<Line>) -> String {
    let s = stroke.map(|l| format!(" {} stroke-width=\"{}\"", l.paint.attr("stroke"), n(l.w))).unwrap_or_default();
    format!(r#"<circle cx="{}" cy="{}" r="{}" {}{s}/>"#, n(cx), n(cy), n(r), fill.attr("fill"))
}

/// Open stroked path through `points` with round caps and joins (glyphs such as a check mark).
pub fn polyline(points: &[(f64, f64)], stroke: Line) -> String {
    let d: Vec<String> = points.iter().enumerate().map(|(i, (x, y))| format!("{}{} {}", if i == 0 { "M" } else { "L" }, n(*x), n(*y))).collect();
    format!(
        r#"<path d="{}" fill="none" {} stroke-width="{}" stroke-linecap="round" stroke-linejoin="round"/>"#,
        d.join(" "),
        stroke.paint.attr("stroke"),
        n(stroke.w)
    )
}

pub fn rrect(x: f64, y: f64, w: f64, h: f64, r: f64, fill: Paint, stroke: Option<Line>) -> String {
    let s = stroke.map(|l| format!(" {} stroke-width=\"{}\"", l.paint.attr("stroke"), n(l.w))).unwrap_or_default();
    format!(r#"<rect x="{}" y="{}" width="{}" height="{}" rx="{}" {}{s}/>"#, n(x), n(y), n(w), n(h), n(r), fill.attr("fill"))
}

pub fn rasterize(svg: &str, w: u32, h: u32) -> anyhow::Result<Vec<u8>> {
    let tree = resvg::usvg::Tree::from_str(svg, &resvg::usvg::Options::default())?;
    let mut pm = tiny_skia::Pixmap::new(w, h).ok_or_else(|| anyhow::anyhow!("bad size {w}x{h}"))?;
    let size = tree.size();
    let tf = tiny_skia::Transform::from_scale(w as f32 / size.width(), h as f32 / size.height());
    resvg::render(&tree, tf, &mut pm.as_mut());
    Ok(pm.encode_png()?)
}

pub fn base64(data: &[u8]) -> String {
    const T: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::with_capacity(data.len().div_ceil(3) * 4);
    for chunk in data.chunks(3) {
        let b = [chunk[0], *chunk.get(1).unwrap_or(&0), *chunk.get(2).unwrap_or(&0)];
        let v = (b[0] as u32) << 16 | (b[1] as u32) << 8 | b[2] as u32;
        out.push(T[(v >> 18) as usize & 63] as char);
        out.push(T[(v >> 12) as usize & 63] as char);
        out.push(if chunk.len() > 1 { T[(v >> 6) as usize & 63] as char } else { '=' });
        out.push(if chunk.len() > 2 { T[v as usize & 63] as char } else { '=' });
    }
    out
}

/// One Fluent shadow layer: offset x/y, CSS blur radius, alpha.
#[derive(Clone, Copy, Debug)]
pub struct ShadowLayer {
    pub x: f64,
    pub y: f64,
    pub blur: f64,
    pub alpha: f64,
}

/// Rendered shadow: 8 PNG tiles + paddings (l, t, r, b).
pub struct ShadowTiles {
    pub pad: [f64; 4],
    pub tiles: Vec<(&'static str, f64, f64, Vec<u8>)>,
}

fn erfc(x: f64) -> f64 {
    // Abramowitz–Stegun 7.1.26, |err| < 1.5e-7
    let z = x.abs();
    let t = 1.0 / (1.0 + 0.3275911 * z);
    let y = t * (0.254829592 + t * (-0.284496736 + t * (1.421413741 + t * (-1.453152027 + t * 1.061405429)))) * (-z * z).exp();
    if x >= 0.0 { y } else { 2.0 - y }
}

/// Coverage of a gaussian-blurred half-plane at signed inside distance `d`.
fn g(d: f64, blur: f64) -> f64 {
    if blur <= 0.0 {
        return if d >= 0.0 { 1.0 } else { 0.0 };
    }
    let sigma = blur / 2.0;
    0.5 * erfc(-d / (sigma * std::f64::consts::SQRT_2))
}

/// Shadow of a rounded window of corner radius `radius`. Tiles exclude the window interior so
/// translucent surfaces never darken over their own shadow.
pub fn shadow_tiles(layers: &[ShadowLayer], radius: f64, color: Rgba) -> ShadowTiles {
    let ext = |f: &dyn Fn(&ShadowLayer) -> f64| layers.iter().map(|l| (l.blur + f(l)).max(0.0)).fold(0.0f64, f64::max).ceil();
    let pad = [ext(&|l| -l.x), ext(&|l| -l.y), ext(&|l| l.x), ext(&|l| l.y)];
    let c = radius.ceil().max(1.0);
    let intensity = |u: f64, v: f64, sx: f64, sy: f64, edge_only: (bool, bool)| -> f64 {
        let mut keep = 1.0;
        for l in layers {
            let gu = if edge_only.0 { 1.0 } else { g(u + sx * l.x, l.blur) };
            let gv = if edge_only.1 { 1.0 } else { g(v + sy * l.y, l.blur) };
            keep *= 1.0 - l.alpha * gu * gv;
        }
        let a = 1.0 - keep;
        let uu = if edge_only.0 { f64::INFINITY } else { u };
        let vv = if edge_only.1 { f64::INFINITY } else { v };
        let sdf = if uu < radius && vv < radius {
            ((radius - uu).powi(2) + (radius - vv).powi(2)).sqrt() - radius
        } else {
            (-uu).max(-vv)
        };
        a * (sdf + 0.5).clamp(0.0, 1.0)
    };

    let render = |w: f64, h: f64, f: &dyn Fn(f64, f64) -> f64| -> Vec<u8> {
        let (w, h) = (w.max(1.0) as u32, h.max(1.0) as u32);
        let mut pm = tiny_skia::Pixmap::new(w, h).expect("pixmap");
        let [r, gg, b, _] = color.u8s();
        for (i, px) in pm.pixels_mut().iter_mut().enumerate() {
            let (x, y) = ((i as u32 % w) as f64 + 0.5, (i as u32 / w) as f64 + 0.5);
            let a = (f(x, y) * color.a).clamp(0.0, 1.0);
            let pa = (a * 255.0).round() as u8;
            let pm_ = |ch: u8| ((ch as f64) * a).round() as u8;
            *px = tiny_skia::PremultipliedColorU8::from_rgba(pm_(r), pm_(gg), pm_(b), pa).unwrap();
        }
        pm.encode_png().expect("png")
    };

    let [pl, pt, pr, pb] = pad;
    let ew = 8.0;
    let mut tiles = Vec::with_capacity(8);
    // u: inside distance from vertical edge, v: from horizontal edge; sx/sy: -1 left/top, +1 right/bottom.
    tiles.push(("topleft", pl + c, pt + c, render(pl + c, pt + c, &|x, y| intensity(x - pl, y - pt, -1.0, -1.0, (false, false)))));
    tiles.push(("topright", pr + c, pt + c, render(pr + c, pt + c, &|x, y| intensity(c - x, y - pt, 1.0, -1.0, (false, false)))));
    tiles.push(("bottomleft", pl + c, pb + c, render(pl + c, pb + c, &|x, y| intensity(x - pl, c - y, -1.0, 1.0, (false, false)))));
    tiles.push(("bottomright", pr + c, pb + c, render(pr + c, pb + c, &|x, y| intensity(c - x, c - y, 1.0, 1.0, (false, false)))));
    tiles.push(("top", ew, pt, render(ew, pt, &|_, y| intensity(0.0, y - pt, -1.0, -1.0, (true, false)))));
    tiles.push(("bottom", ew, pb, render(ew, pb, &|_, y| intensity(0.0, -y, -1.0, 1.0, (true, false)))));
    tiles.push(("left", pl, ew, render(pl, ew, &|x, _| intensity(x - pl, 0.0, -1.0, -1.0, (false, true)))));
    tiles.push(("right", pr, ew, render(pr, ew, &|x, _| intensity(-x, 0.0, 1.0, -1.0, (false, true)))));
    ShadowTiles { pad, tiles }
}

impl Svg {
    /// `shadow-*` tiles + `shadow-hint-*-margin`.
    pub fn shadow(&mut self, s: &ShadowTiles) {
        for (name, w, h, png) in &s.tiles {
            self.image(&format!("shadow-{name}"), *w, *h, png);
        }
        let [l, t, r, b] = s.pad;
        self.hint("shadow-hint-left-margin", l, 4.0);
        self.hint("shadow-hint-top-margin", 4.0, t);
        self.hint("shadow-hint-right-margin", r, 4.0);
        self.hint("shadow-hint-bottom-margin", 4.0, b);
    }
}
