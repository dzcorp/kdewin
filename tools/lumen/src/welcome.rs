//! After a package install: one offer per user, in the user's own session, to apply Lumen.
//! Strings live in the kcm_lumen catalog (`lumen i18n` extracts this file as C: keep it free of char literals).

use crate::ModeArg;
use crate::ctx::{Ctx, write_file};
use crate::install::{apply, data_home, output, try_run};
use anyhow::Result;
use std::path::Path;
use std::process::Command;

const MARKER: &str = "lumen/welcomed";

fn i18n(text: &str) -> String {
    Command::new("gettext")
        .args(["-d", "kcm_lumen", "--", text])
        .output()
        .ok()
        .filter(|o| o.status.success() && !o.stdout.is_empty())
        .map_or_else(|| text.to_owned(), |o| String::from_utf8_lossy(&o.stdout).into_owned())
}

/// Shows a notification; with actions, waits and returns the chosen action id (empty when dismissed).
fn notify(summary: &str, body: &str, actions: &[(&str, String)]) -> String {
    let mut cmd = Command::new("notify-send");
    cmd.args(["--app-name=Lumen", "--icon=preferences-desktop-theme-global"]);
    if !actions.is_empty() {
        cmd.arg("--expire-time=0");
        for (id, label) in actions {
            cmd.arg(format!("--action={id}={label}"));
        }
    }
    cmd.arg(summary).arg(body);
    cmd.output().map(|o| String::from_utf8_lossy(&o.stdout).trim().to_owned()).unwrap_or_default()
}

/// Run by the autostart entry at every login and by the package hook in running sessions after an install or
/// upgrade. Where Lumen is active it re-applies a newer version once; elsewhere it offers Lumen once per user
/// (`lumen uninstall` removes the marker, so a reinstall asks again).
pub fn welcome(ctx: &Ctx) -> Result<()> {
    let active = output("kreadconfig6", &["--file", "kdeglobals", "--group", "KDE", "--key", "LookAndFeelPackage"]).is_some_and(|v| v.starts_with("org.lumen."));
    if active {
        return update(ctx);
    }
    let marker = data_home().join(MARKER);
    if marker.exists() {
        return Ok(());
    }
    write_file(&marker, b"")?;
    let choice = notify(
        &i18n("Lumen is installed"),
        &i18n("Switch this session to the Windows 11 look? You can change it later in System Settings."),
        &[("apply", i18n("Apply Lumen")), ("layout", i18n("Apply with taskbar")), ("later", i18n("Not now"))],
    );
    let layout = match choice.as_str() {
        "apply" => false,
        "layout" => true,
        _ => return Ok(()),
    };
    match apply(ctx, ModeArg::Auto, layout) {
        Ok(()) => {
            // The login screen needs root: the session's polkit agent asks for the password.
            if Path::new("/etc/plasmalogin.conf").exists()
                && let Ok(exe) = std::env::current_exe()
            {
                let _ = Command::new("pkexec").arg(exe).args(["login", "on"]).status();
            }
            notify(&i18n("Lumen is on"), &i18n("Log out and back in to finish."), &[]);
        }
        Err(e) => {
            notify(&i18n("Lumen could not be applied"), &format!("{e:#}"), &[]);
        }
    }
    Ok(())
}

/// After an upgrade: new defaults and settings reach the session by applying again, in the user's mode and without
/// touching the panel layout.
fn update(ctx: &Ctx) -> Result<()> {
    let read = |group: &str, key: &str| output("kreadconfig6", &["--file", "lumenrc", "--group", group, "--key", key]).unwrap_or_default();
    if read("Restore", "AppliedVersion") == ctx.version() {
        return Ok(());
    }
    let mode = match read("Appearance", "Mode").as_str() {
        "dark" => ModeArg::Dark,
        "light" => ModeArg::Light,
        _ => ModeArg::Auto,
    };
    match apply(ctx, mode, false) {
        Ok(()) => notify(&i18n("Lumen is updated"), &i18n("Log out and back in to finish."), &[]),
        Err(e) => notify(&i18n("Lumen could not be applied"), &format!("{e:#}"), &[]),
    };
    Ok(())
}

/// pacman hook after the package is installed or upgraded (root): runs `lumen welcome` in every running graphical
/// session.
pub fn post_install() -> Result<()> {
    let sessions = output("loginctl", &["list-sessions", "--no-legend"]).unwrap_or_default();
    let mut users: Vec<String> = Vec::new();
    for id in sessions.lines().filter_map(|l| l.split_whitespace().next()) {
        let props = output("loginctl", &["show-session", id, "--property=Name", "--property=Type", "--property=Class"]).unwrap_or_default();
        let get = |key: &str| props.lines().find_map(|l| l.strip_prefix(key)).unwrap_or_default().to_owned();
        let (name, kind) = (get("Name="), get("Type="));
        if get("Class=") == "user" && (kind == "wayland" || kind == "x11") && !name.is_empty() && !users.contains(&name) {
            users.push(name);
        }
    }
    let exe = std::env::current_exe()?.to_string_lossy().into_owned();
    for user in &users {
        let machine = format!("--machine={user}@.host");
        try_run("systemd-run", &["--user", &machine, "--collect", "--quiet", &exe, "welcome"]);
    }
    Ok(())
}
