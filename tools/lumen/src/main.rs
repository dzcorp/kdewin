mod color;
mod ctx;
mod generate;
mod i18n;
mod install;
mod layout;
mod svg;
mod tokens;
mod upstream;
mod version;
mod welcome;

use anyhow::Result;
use clap::{Parser, Subcommand, ValueEnum};
use std::path::PathBuf;

#[derive(Parser)]
#[command(name = "lumen", version, about = "Lumen KDE theme: generate, build, install")]
struct Cli {
    /// Repository root (defaults to nearest parent containing tokens/tokens.toml)
    #[arg(long, global = true)]
    root: Option<PathBuf>,
    #[command(subcommand)]
    cmd: Cmd,
}

#[derive(Subcommand)]
enum Cmd {
    /// Generate all assets from tokens into build/
    Gen,
    /// Generate and compile native components
    Build,
    /// Install user data (and native/system parts with --system)
    Install {
        /// Also install native plugins and system parts (sudo): lock/logout overlay, pacman hooks, boot, login screen
        #[arg(long)]
        system: bool,
        /// Use the existing build/out and build/cmake (no gen, no compile)
        #[arg(long)]
        no_build: bool,
        /// Skip Plymouth and Limine (boot chain)
        #[arg(long)]
        no_boot: bool,
        /// Stage every system file under this directory for a package (pacman's $pkgdir): no build, no root,
        /// no changes to the running system
        #[arg(long, value_name = "DIR", conflicts_with_all = ["system", "no_boot"])]
        destdir: Option<PathBuf>,
    },
    /// Activate Lumen in a color mode
    Apply {
        #[arg(value_enum, default_value_t = ModeArg::Auto)]
        mode: ModeArg,
        /// Also replace the panel/desktop layout with the Lumen taskbar layout
        #[arg(long)]
        layout: bool,
    },
    /// Switch this session back to Breeze and remove Lumen's user files (and a source install's system files with --system)
    Uninstall {
        #[arg(long)]
        system: bool,
    },
    /// Undo what Lumen changed outside the package's own files: boot, login screen, shell copy (pacman pre_remove, root)
    #[command(hide = true)]
    PreRemove,
    /// Offer to apply Lumen once per user (autostart after a package install)
    #[command(hide = true)]
    Welcome,
    /// Run `lumen welcome` in every running graphical session (pacman hook, root)
    #[command(hide = true)]
    PostInstall,
    /// Print the release version and check every copy matches it; with a version, set it everywhere
    Version {
        /// New version (MAJOR.MINOR.PATCH)
        set: Option<String>,
    },
    /// Merge the forked KDE sources with a newer upstream release (forks listed in upstream/forks.toml)
    Upstream {
        #[command(subcommand)]
        cmd: UpstreamCmd,
    },
    /// Check installation health, fall back to stock blur if the effect failed to load
    Doctor,
    /// Remove installed packages Lumen supersedes (asks first)
    Cleanup {
        #[arg(long)]
        yes: bool,
    },
    /// Rebuild and reinstall the native plugins (pacman hook, runs as root)
    RebuildKwin,
    /// Lumen boot splash (Plymouth) and boot menu (Limine) theme (sudo)
    Boot {
        #[arg(value_enum)]
        state: Toggle,
    },
    /// Lumen wallpaper and theme on the Plasma Login Manager greeter (sudo)
    Login {
        #[arg(value_enum)]
        state: Toggle,
    },
    /// Copy Segoe UI, Segoe UI Variable, Segoe Fluent Icons and Consolas from your Windows installation into
    /// fonts/windows (git-ignored) and install them system-wide (sudo); `install` reinstalls them, `apply` uses them
    Fonts {
        /// Windows partition root, or its Windows/Fonts folder
        path: PathBuf,
    },
    /// Print what `apply --layout` would carry over from the current panel (no changes)
    Snapshot,
    /// Reload the desktop shell, search and file dialogs and refresh icon caches (after icon/theme changes made elsewhere)
    Restart,
    /// Re-copy the stock shell package plus the Lumen lock/logout overlay and rebuild the Kirigami forms patch (pacman hook, runs as root)
    RefreshShell,
    /// Extract strings and fill po/ from KDE's installed translations
    I18n {
        /// Print one language's untranslated strings as a .po skeleton instead
        #[arg(long, value_name = "LANG")]
        todo: Option<String>,
    },
}

#[derive(Subcommand)]
enum UpstreamCmd {
    /// Print each fork's version and the newest upstream release
    Check,
    /// Three-way merge forks with the newest release (or --to); exit code 3 when conflicts are left in the tree
    Sync {
        /// Only this fork (section name in upstream/forks.toml)
        fork: Option<String>,
        /// Target version instead of the newest release
        #[arg(long, requires = "fork")]
        to: Option<String>,
        /// Write a Markdown summary of the merge here
        #[arg(long, value_name = "FILE")]
        report: Option<PathBuf>,
    },
}

#[derive(Clone, Copy, ValueEnum)]
pub enum Toggle {
    On,
    Off,
}

#[derive(Clone, Copy, ValueEnum)]
pub enum ModeArg {
    Dark,
    Light,
    Auto,
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    let ctx = ctx::Ctx::new(cli.root)?;
    match cli.cmd {
        Cmd::Gen => generate::run(&ctx),
        Cmd::Build => {
            generate::run(&ctx)?;
            install::build_native(&ctx)
        }
        Cmd::Install { destdir: Some(dest), .. } => install::stage(&ctx, &dest),
        Cmd::Install { system, no_build, no_boot, destdir: None } => {
            if !no_build {
                generate::run(&ctx)?;
                if system {
                    install::build_native(&ctx)?;
                }
            }
            install::install_user(&ctx)?;
            if system {
                install::install_system(&ctx, no_boot)?;
            }
            Ok(())
        }
        Cmd::Apply { mode, layout } => install::apply(&ctx, mode, layout),
        Cmd::Uninstall { system } => install::uninstall(&ctx, system),
        Cmd::PreRemove => install::pre_remove(&ctx),
        Cmd::Welcome => welcome::welcome(&ctx),
        Cmd::PostInstall => welcome::post_install(),
        Cmd::Version { set } => version::run(&ctx, set.as_deref()),
        Cmd::Upstream { cmd: UpstreamCmd::Check } => upstream::check(&ctx),
        Cmd::Upstream { cmd: UpstreamCmd::Sync { fork, to, report } } => {
            if upstream::sync(&ctx, fork.as_deref(), to.as_deref(), report.as_deref())? > 0 {
                std::process::exit(3);
            }
            Ok(())
        }
        Cmd::Doctor => install::doctor(&ctx),
        Cmd::Cleanup { yes } => install::cleanup(&ctx, yes),
        Cmd::RebuildKwin => install::rebuild_kwin(&ctx),
        Cmd::RefreshShell => install::refresh_shell(&ctx),
        Cmd::Restart => install::restart(),
        Cmd::Fonts { path } => install::import_fonts(&ctx, &path),
        Cmd::Boot { state } => install::boot(&ctx, matches!(state, Toggle::On)),
        Cmd::Login { state } => install::login(&ctx, matches!(state, Toggle::On)),
        Cmd::I18n { todo } => match todo {
            Some(lang) => i18n::todo(&ctx, &lang),
            None => i18n::run(&ctx),
        },
        Cmd::Snapshot => {
            print!("{}", layout::render(&layout::snapshot(&layout::appletsrc())));
            Ok(())
        }
    }
}
