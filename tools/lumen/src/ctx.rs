use crate::tokens::Tokens;
use anyhow::{Context, Result, bail};
use std::path::{Path, PathBuf};

/// Read-only data root of a packaged install (`lumen install --destdir`).
pub const PACKAGED_ROOT: &str = "/usr/share/lumen/root";

pub struct Ctx {
    pub root: PathBuf,
    pub gen_dir: PathBuf,
    pub out: PathBuf,
    pub t: Tokens,
    /// Installed by pacman: no sources, no build tree, `gen_dir` is a cache.
    pub packaged: bool,
}

impl Ctx {
    pub fn new(root: Option<PathBuf>) -> Result<Self> {
        let root = match root.or_else(|| std::env::var_os("LUMEN_ROOT").map(PathBuf::from)) {
            Some(r) => r,
            None => find_root()?,
        };
        let root = root.canonicalize().with_context(|| format!("root {}", root.display()))?;
        let t = Tokens::load(&root.join("tokens/tokens.toml"))?;
        let packaged = root == Path::new(PACKAGED_ROOT);
        let gen_dir = if !packaged {
            root.join("build/gen")
        } else if is_root() {
            PathBuf::from("/var/cache/lumen")
        } else {
            std::env::var_os("XDG_CACHE_HOME")
                .map(PathBuf::from)
                .or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".cache")))
                .context("HOME")?
                .join("lumen")
        };
        Ok(Self { gen_dir, out: root.join("build/out"), root, t, packaged })
    }

    pub fn src(&self, rel: impl AsRef<Path>) -> PathBuf {
        self.root.join(rel)
    }

    pub fn out_path(&self, rel: impl AsRef<Path>) -> PathBuf {
        self.out.join(rel)
    }

    pub fn write(&self, rel: impl AsRef<Path>, data: impl AsRef<[u8]>) -> Result<()> {
        write_file(&self.out.join(rel), data.as_ref())
    }

    pub fn write_gen(&self, rel: impl AsRef<Path>, data: impl AsRef<[u8]>) -> Result<()> {
        write_file(&self.gen_dir.join(rel), data.as_ref())
    }

    pub fn version(&self) -> &str {
        self.t.s("meta.version")
    }

    /// Rasterize `svg` to `rel` (PNG, w×h); skipped when source and size are unchanged.
    pub fn render(&self, rel: impl AsRef<Path>, svg: &str, w: u32, h: u32) -> Result<()> {
        self.cached(rel, fnv1a(svg.as_bytes()) ^ ((w as u64) << 32 | h as u64), || crate::svg::rasterize(svg, w, h))
    }

    /// Write `make()` to `rel` unless the output exists and was built from the same `key`.
    pub fn cached(&self, rel: impl AsRef<Path>, key: u64, make: impl FnOnce() -> Result<Vec<u8>>) -> Result<()> {
        let rel = rel.as_ref();
        let stamp = self.gen_dir.join("cache").join(rel).with_extension("hash");
        let hash = format!("{key:016x}");
        let out = self.out.join(rel);
        if out.is_file() && std::fs::read_to_string(&stamp).is_ok_and(|s| s == hash) {
            return Ok(());
        }
        write_file(&out, &make()?)?;
        write_file(&stamp, hash.as_bytes())
    }
}

pub fn fnv1a(data: &[u8]) -> u64 {
    data.iter().fold(0xcbf29ce484222325u64, |h, b| (h ^ *b as u64).wrapping_mul(0x100000001b3))
}

pub fn write_file(path: &Path, data: &[u8]) -> Result<()> {
    if let Some(dir) = path.parent() {
        std::fs::create_dir_all(dir).with_context(|| format!("mkdir {}", dir.display()))?;
    }
    if std::fs::read(path).is_ok_and(|old| old == data) {
        return Ok(());
    }
    std::fs::write(path, data).with_context(|| format!("write {}", path.display()))
}

/// /proc/self belongs to the effective user.
pub fn is_root() -> bool {
    use std::os::unix::fs::MetadataExt;
    std::fs::metadata("/proc/self").is_ok_and(|m| m.uid() == 0)
}

fn find_root() -> Result<PathBuf> {
    let mut dir = std::env::current_dir()?;
    loop {
        if dir.join("tokens/tokens.toml").is_file() {
            return Ok(dir);
        }
        if !dir.pop() {
            break;
        }
    }
    let packaged = PathBuf::from(PACKAGED_ROOT);
    if packaged.join("tokens/tokens.toml").is_file() {
        return Ok(packaged);
    }
    bail!("tokens/tokens.toml not found; pass --root or set LUMEN_ROOT")
}
