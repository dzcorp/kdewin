mod assemble;
pub use assemble::{KIRIGAMI_STYLE_OUT, QQC2_OUT};
mod boot;
mod colors;
mod cpp;
mod desktoptheme;
mod firefox;
mod gtk;
pub mod kcfg;
mod qml;
mod terminals;
mod union;
mod wallpaper;

pub use qml::qml_tokens;

use crate::ctx::Ctx;
use anyhow::Result;
use std::time::Instant;

type Step = (&'static str, fn(&Ctx) -> Result<()>);

const STEPS: &[Step] = &[
    ("cpp tokens", cpp::generate),
    ("settings schema", kcfg::generate),
    ("color schemes", colors::generate),
    ("plasma style", desktoptheme::generate),
    ("gtk", gtk::generate),
    ("firefox", firefox::generate),
    ("union", union::generate),
    ("terminals", terminals::generate),
    ("wallpaper", wallpaper::generate),
    ("boot", boot::generate),
    ("packages", assemble::generate),
];

pub fn run(ctx: &Ctx) -> Result<()> {
    let all = Instant::now();
    let (independent, last) = STEPS.split_at(STEPS.len() - 1);
    let timed = |(name, step): &Step| -> Result<(&'static str, f64)> {
        let t = Instant::now();
        step(ctx)?;
        Ok((*name, t.elapsed().as_secs_f64() * 1e3))
    };
    let mut times = std::thread::scope(|scope| {
        let jobs: Vec<_> = independent.iter().map(|s| scope.spawn(move || timed(s))).collect();
        jobs.into_iter().map(|j| j.join().expect("gen step thread")).collect::<Result<Vec<_>>>()
    })?;
    times.push(timed(&last[0])?);
    for (name, ms) in times {
        println!("  {name:<16} {ms:>7.1} ms");
    }
    println!("gen done in {:.1} ms → {}", all.elapsed().as_secs_f64() * 1e3, ctx.out.display());
    Ok(())
}
