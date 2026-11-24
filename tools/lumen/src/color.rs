use crate::tokens::Tokens;
use anyhow::{Result, bail};

/// Straight (non-premultiplied) sRGB color, channels in 0..=1.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Rgba {
    pub r: f64,
    pub g: f64,
    pub b: f64,
    pub a: f64,
}

impl Rgba {
    pub const BLACK: Rgba = Rgba { r: 0.0, g: 0.0, b: 0.0, a: 1.0 };
    pub const WHITE: Rgba = Rgba { r: 1.0, g: 1.0, b: 1.0, a: 1.0 };

    pub fn rgb8(r: u8, g: u8, b: u8) -> Self {
        Self { r: r as f64 / 255.0, g: g as f64 / 255.0, b: b as f64 / 255.0, a: 1.0 }
    }

    /// `#RGB`, `#RRGGBB` or `#RRGGBBAA`.
    pub fn parse(s: &str) -> Result<Self> {
        let h = s.strip_prefix('#').unwrap_or(s);
        let byte = |i: usize| u8::from_str_radix(&h[i..i + 2], 16);
        let c = match h.len() {
            3 => {
                let n = |i: usize| u8::from_str_radix(&h[i..i + 1], 16).map(|v| v * 17);
                Self::rgb8(n(0)?, n(1)?, n(2)?)
            }
            6 => Self::rgb8(byte(0)?, byte(2)?, byte(4)?),
            8 => Self::rgb8(byte(0)?, byte(2)?, byte(4)?).alpha(byte(6)? as f64 / 255.0),
            _ => bail!("bad color '{s}'"),
        };
        Ok(c)
    }

    pub fn alpha(self, a: f64) -> Self {
        Self { a: a.clamp(0.0, 1.0), ..self }
    }

    pub fn u8s(self) -> [u8; 4] {
        let q = |v: f64| (v.clamp(0.0, 1.0) * 255.0).round() as u8;
        [q(self.r), q(self.g), q(self.b), q(self.a)]
    }

    pub fn hex(self) -> String {
        let [r, g, b, _] = self.u8s();
        format!("#{r:02x}{g:02x}{b:02x}")
    }

    /// `#aarrggbb` (QML / Qt order).
    pub fn argb(self) -> String {
        let [r, g, b, a] = self.u8s();
        format!("#{a:02x}{r:02x}{g:02x}{b:02x}")
    }

    pub fn argb_u32(self) -> u32 {
        let [r, g, b, a] = self.u8s();
        u32::from_be_bytes([a, r, g, b])
    }

    /// CSS value: hex when opaque, rgba() otherwise.
    pub fn css(self) -> String {
        if self.a >= 1.0 {
            self.hex()
        } else {
            let [r, g, b, _] = self.u8s();
            format!("rgba({r}, {g}, {b}, {})", crate::tokens::fmt_num((self.a * 1000.0).round() / 1000.0))
        }
    }

    /// KColorScheme `r,g,b`.
    pub fn kde(self) -> String {
        let [r, g, b, _] = self.u8s();
        format!("{r},{g},{b}")
    }

    /// KColorScheme `r,g,b,a` (used in [WM]).
    /// Source-over composite onto `bg`.
    pub fn over(self, bg: Rgba) -> Rgba {
        let a = self.a + bg.a * (1.0 - self.a);
        if a <= 0.0 {
            return Rgba { a: 0.0, ..bg };
        }
        let ch = |s: f64, d: f64| (s * self.a + d * bg.a * (1.0 - self.a)) / a;
        Rgba { r: ch(self.r, bg.r), g: ch(self.g, bg.g), b: ch(self.b, bg.b), a }
    }

    pub fn mix(self, o: Rgba, t: f64) -> Rgba {
        let l = |x: f64, y: f64| x + (y - x) * t;
        Rgba { r: l(self.r, o.r), g: l(self.g, o.g), b: l(self.b, o.b), a: l(self.a, o.a) }
    }

    pub fn oklab(self) -> [f64; 3] {
        let (r, g, b) = (to_lin(self.r), to_lin(self.g), to_lin(self.b));
        let l = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b).cbrt();
        let m = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b).cbrt();
        let s = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b).cbrt();
        [
            0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
        ]
    }

    /// Returns linear-light channels which may fall outside 0..=1 (gamut check).
    fn from_oklab_raw(lab: [f64; 3]) -> [f64; 3] {
        let [ll, a, b] = lab;
        let l = (ll + 0.3963377774 * a + 0.2158037573 * b).powi(3);
        let m = (ll - 0.1055613458 * a - 0.0638541728 * b).powi(3);
        let s = (ll - 0.0894841775 * a - 1.2914855480 * b).powi(3);
        [
            4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
            -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
            -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
        ]
    }

    pub fn lightness(self) -> f64 {
        self.oklab()[0]
    }

    /// Same hue/chroma at OKLab lightness `l`, chroma reduced until inside sRGB.
    pub fn with_lightness(self, l: f64) -> Rgba {
        let [_, a, b] = self.oklab();
        let mut k = 1.0;
        for _ in 0..64 {
            let lin = Self::from_oklab_raw([l, a * k, b * k]);
            if lin.iter().all(|c| (-1e-4..=1.0 + 1e-4).contains(c)) {
                return Rgba { r: from_lin(lin[0]), g: from_lin(lin[1]), b: from_lin(lin[2]), a: self.a };
            }
            k *= 0.95;
        }
        let lin = Self::from_oklab_raw([l, 0.0, 0.0]);
        Rgba { r: from_lin(lin[0]), g: from_lin(lin[1]), b: from_lin(lin[2]), a: self.a }
    }
}

fn to_lin(c: f64) -> f64 {
    if c <= 0.04045 { c / 12.92 } else { ((c + 0.055) / 1.055).powf(2.4) }
}

fn from_lin(c: f64) -> f64 {
    let c = c.clamp(0.0, 1.0);
    if c <= 0.0031308 { 12.92 * c } else { 1.055 * c.powf(1.0 / 2.4) - 0.055 }
}

#[derive(Clone, Copy, Debug)]
pub struct Shades {
    pub light3: Rgba,
    pub light2: Rgba,
    pub base: Rgba,
    pub dark1: Rgba,
    pub dark2: Rgba,
}

impl Shades {
    /// Fluent: accent fill = light2 in dark mode, dark1 in light mode.
    pub fn fill(&self, dark: bool) -> Rgba {
        if dark { self.light2 } else { self.dark1 }
    }

    /// Accent-colored text on the base background.
    pub fn text(&self, dark: bool) -> Rgba {
        if dark { self.light3 } else { self.dark2 }
    }
}

pub fn accent_shades(t: &Tokens, base: Rgba) -> Shades {
    let default = t.color("accent.default");
    if base.hex() == default.hex() {
        return Shades {
            light3: t.color("accent.light3"),
            light2: t.color("accent.light2"),
            base,
            dark1: t.color("accent.dark1"),
            dark2: t.color("accent.dark2"),
        };
    }
    let lb = base.lightness();
    let lmax = t.f("accent.lmax");
    let lf = t.fv("accent.light_f");
    let df = t.fv("accent.dark_f");
    let light = |f: f64| base.with_lightness(lb + (lmax - lb) * f);
    let dark = |f: f64| base.with_lightness(lb * f);
    Shades {
        light3: light(lf[2]),
        light2: light(lf[1]),
        base,
        dark1: dark(df[0]),
        dark2: dark(df[1]),
    }
}
