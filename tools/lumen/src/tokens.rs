use crate::color::Rgba;
use anyhow::{Context, Result};
use std::path::Path;
use toml::Value;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Mode {
    Dark,
    Light,
}

impl Mode {
    pub const ALL: [Mode; 2] = [Mode::Dark, Mode::Light];

    pub fn name(self) -> &'static str {
        match self {
            Mode::Dark => "dark",
            Mode::Light => "light",
        }
    }

    pub fn title(self) -> &'static str {
        match self {
            Mode::Dark => "Dark",
            Mode::Light => "Light",
        }
    }
}

/// Token lookups panic on missing keys: tokens.toml is part of the source tree.
pub struct Tokens {
    root: Value,
}

impl Tokens {
    pub fn load(path: &Path) -> Result<Self> {
        let text = std::fs::read_to_string(path).with_context(|| format!("read {}", path.display()))?;
        let root: Value = toml::from_str(&text).with_context(|| format!("parse {}", path.display()))?;
        Ok(Self { root })
    }

    pub fn try_get(&self, path: &str) -> Option<&Value> {
        path.split('.').try_fold(&self.root, |v, k| v.get(k))
    }

    pub fn get(&self, path: &str) -> &Value {
        self.try_get(path).unwrap_or_else(|| panic!("token missing: {path}"))
    }

    pub fn f(&self, path: &str) -> f64 {
        as_f64(self.get(path)).unwrap_or_else(|| panic!("token not numeric: {path}"))
    }

    pub fn i(&self, path: &str) -> i64 {
        self.f(path).round() as i64
    }

    pub fn s(&self, path: &str) -> &str {
        self.get(path).as_str().unwrap_or_else(|| panic!("token not string: {path}"))
    }

    pub fn color(&self, path: &str) -> Rgba {
        Rgba::parse(self.s(path)).unwrap_or_else(|e| panic!("token {path}: {e}"))
    }

    pub fn fv(&self, path: &str) -> Vec<f64> {
        self.get(path)
            .as_array()
            .unwrap_or_else(|| panic!("token not array: {path}"))
            .iter()
            .map(|v| as_f64(v).unwrap_or_else(|| panic!("token array not numeric: {path}")))
            .collect()
    }

    pub fn sv(&self, path: &str) -> Vec<String> {
        self.get(path)
            .as_array()
            .unwrap_or_else(|| panic!("token not array: {path}"))
            .iter()
            .map(|v| v.as_str().unwrap_or_else(|| panic!("token array not string: {path}")).to_owned())
            .collect()
    }

    /// Nested numeric arrays, e.g. shadow layers.
    pub fn fvv(&self, path: &str) -> Vec<Vec<f64>> {
        self.get(path)
            .as_array()
            .unwrap_or_else(|| panic!("token not array: {path}"))
            .iter()
            .map(|row| {
                row.as_array()
                    .unwrap_or_else(|| panic!("token row not array: {path}"))
                    .iter()
                    .map(|v| as_f64(v).unwrap_or_else(|| panic!("token not numeric: {path}")))
                    .collect()
            })
            .collect()
    }

    pub fn table(&self, path: &str) -> &toml::Table {
        self.get(path).as_table().unwrap_or_else(|| panic!("token not table: {path}"))
    }

    /// Fluent semantic color `color.<mode>.<key>`.
    pub fn c(&self, m: Mode, key: &str) -> Rgba {
        self.color(&format!("color.{}.{key}", m.name()))
    }

    /// Solid palette role `palette.<mode>.<key>`.
    pub fn p(&self, m: Mode, key: &str) -> Rgba {
        self.color(&format!("palette.{}.{key}", m.name()))
    }

    /// Per-mode numeric value `<path>.<mode>`.
    pub fn fm(&self, path: &str, m: Mode) -> f64 {
        self.f(&format!("{path}.{}", m.name()))
    }

    /// Stringify a token for `@@path@@` placeholder substitution.
    pub fn placeholder(&self, path: &str) -> Option<String> {
        self.try_get(path).and_then(value_text)
    }
}

fn as_f64(v: &Value) -> Option<f64> {
    v.as_float().or_else(|| v.as_integer().map(|i| i as f64))
}

fn value_text(v: &Value) -> Option<String> {
    match v {
        Value::String(s) => Some(s.clone()),
        Value::Integer(i) => Some(i.to_string()),
        Value::Float(f) => Some(fmt_num(*f)),
        Value::Boolean(b) => Some(b.to_string()),
        Value::Array(a) => {
            let parts: Option<Vec<String>> = a.iter().map(value_text).collect();
            parts.map(|p| p.join(", "))
        }
        _ => None,
    }
}

pub fn fmt_num(f: f64) -> String {
    if f.fract() == 0.0 && f.abs() < 1e15 {
        format!("{}", f as i64)
    } else {
        let s = format!("{f:.4}");
        s.trim_end_matches('0').trim_end_matches('.').to_owned()
    }
}
