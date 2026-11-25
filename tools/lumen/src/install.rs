use crate::ModeArg;
use crate::ctx::{Ctx, write_file};
use crate::generate::{KIRIGAMI_STYLE_OUT, QQC2_OUT};
use anyhow::{Context, Result, bail};
use std::path::{Path, PathBuf};
use std::process::Command;

const KWIN_HOOK: &str = "/etc/pacman.d/hooks/lumen-kwin.hook";
const SHELL_HOOK: &str = "/etc/pacman.d/hooks/lumen-shell.hook";
const SHELL_TARGETS: &[&str] = &["plasma-desktop", "plasma-workspace", "kscreenlocker", "kirigami", "plasma-login-manager", "plasma-keyboard"];
/// AUR packages built from this tree (`lumen install --destdir`).
const PACKAGES: &[&str] = &["lumen-kde", "lumen-kde-git"];
/// Updates that leave a packaged build's plugins on old private APIs.
const REBUILD_TARGETS: &[&str] = &["kwin", "kdecoration", "qt6-base", "qt6-declarative", "libplasma", "plasma-workspace", "plasma-desktop", "dolphin"];
const ALPM_HOOKS: &str = "/usr/share/libalpm/hooks";
// Packaged Dolphin: outside PATH's /usr/local; the session env script puts its dirs first.
const PACKAGED_DOLPHIN: &str = "/usr/lib/lumen/bin/dolphin";
const PACKAGED_DOLPHIN_DROPIN: &str = "/usr/lib/systemd/user/plasma-dolphin.service.d/lumen.conf";
const PACKAGED_XDG: &str = "/usr/share/lumen/xdg";
const SESSION_ENV: &str = "/etc/xdg/plasma-workspace/env/lumen.sh";
const SHELL_SRC: &str = "/usr/share/plasma/shells/org.kde.plasma.desktop";
// /usr/local/share precedes /usr/share in XDG_DATA_DIRS, so this copy (stock + Lumen lock/logout pieces) wins.
const SHELL_DST: &str = "/usr/local/share/plasma/shells/org.kde.plasma.desktop";
const SYS_STATE: &str = "/usr/share/lumen";
// The Lumen Dolphin (cmake puts it in /usr/local/bin, ahead of the stock one in PATH) also answers FileManager1 activation.
const DOLPHIN_BIN: &str = "/usr/local/bin/dolphin";
const DOLPHIN_DROPIN: &str = "/etc/systemd/user/plasma-dolphin.service.d/lumen.conf";
const PLM_HOME: &str = "/var/lib/plasmalogin";
const GREETER_KWIN_UNIT: &str = "/usr/lib/systemd/user/plasma-login-kwin_wayland.service";
const GREETER_KWIN_DROPIN: &str = "/etc/systemd/user/plasma-login-kwin_wayland.service.d/lumen.conf";
/// Kirigami as installed, except a patched FormLayout (settings forms as Windows cards). Every other entry links to
/// the real module; QML_IMPORT_PATH puts this tree first.
const KIRIGAMI_SHADOW: &str = "/usr/lib/lumen/qml";
const WIN_FONTS: &str = "/usr/local/share/fonts/lumen-windows";
/// Segoe UI, Segoe UI Variable, Segoe Fluent Icons, Segoe MDL2 Assets, Segoe UI Symbol, Consolas.
const WIN_FONT_FILES: &[&str] = &[
    "segoeui.ttf", "segoeuib.ttf", "segoeuii.ttf", "segoeuiz.ttf", "segoeuil.ttf", "seguili.ttf", "segoeuisl.ttf", "seguisli.ttf",
    "seguisb.ttf", "seguisbi.ttf", "seguibl.ttf", "seguibli.ttf", "SegUIVar.ttf", "SegoeIcons.ttf", "segmdl2.ttf", "seguisym.ttf",
    "consola.ttf", "consolab.ttf", "consolai.ttf", "consolaz.ttf",
];

fn home() -> PathBuf {
    PathBuf::from(std::env::var_os("HOME").expect("HOME"))
}

pub(crate) fn data_home() -> PathBuf {
    std::env::var_os("XDG_DATA_HOME").map(PathBuf::from).unwrap_or_else(|| home().join(".local/share"))
}

pub(crate) fn config_home() -> PathBuf {
    std::env::var_os("XDG_CONFIG_HOME").map(PathBuf::from).unwrap_or_else(|| home().join(".config"))
}

use crate::ctx::is_root;

fn run(cmd: &str, args: &[&str]) -> Result<()> {
    let st = Command::new(cmd).args(args).status().with_context(|| format!("spawn {cmd}"))?;
    if !st.success() {
        bail!("{cmd} {} failed: {st}", args.join(" "));
    }
    Ok(())
}

/// Best-effort command (desktop session helpers may be absent).
pub(crate) fn try_run(cmd: &str, args: &[&str]) -> bool {
    Command::new(cmd).args(args).status().is_ok_and(|s| s.success())
}

pub(crate) fn output(cmd: &str, args: &[&str]) -> Option<String> {
    Command::new(cmd).args(args).output().ok().filter(|o| o.status.success()).map(|o| String::from_utf8_lossy(&o.stdout).trim().to_owned())
}

fn root_cmd(cmd: &str, args: &[&str]) -> Result<()> {
    if is_root() {
        run(cmd, args)
    } else {
        let mut v = vec![cmd];
        v.extend_from_slice(args);
        run("sudo", &v)
    }
}

fn kwrite(file: &str, groups: &[&str], key: &str, value: &str) -> Result<()> {
    write_config(file, groups, key, value, false)
}

fn kwrite_notify(file: &str, groups: &[&str], key: &str, value: &str) -> Result<()> {
    write_config(file, groups, key, value, true)
}

fn write_config(file: &str, groups: &[&str], key: &str, value: &str, notify: bool) -> Result<()> {
    let mut args = vec!["--file", file];
    if notify {
        args.push("--notify");
    }
    for g in groups {
        args.push("--group");
        args.push(g);
    }
    args.extend_from_slice(&["--key", key, value]);
    run("kwriteconfig6", &args)
}

fn kdelete(file: &str, groups: &[&str], key: &str) -> Result<()> {
    let mut args = vec!["--file", file];
    for group in groups {
        args.extend(["--group", group]);
    }
    args.extend(["--key", key, "--delete"]);
    run("kwriteconfig6", &args)
}

/// Root-only paths (the ESP) seen through sudo when not root.
fn root_exists(path: &str) -> bool {
    if is_root() { Path::new(path).exists() } else { try_quiet("sudo", &["test", "-e", path]) }
}

fn root_read(path: &str) -> Option<String> {
    if is_root() {
        return std::fs::read_to_string(path).ok();
    }
    Command::new("sudo").args(["cat", path]).output().ok().filter(|o| o.status.success()).map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
}

/// The value under the user's own file (distribution and look-and-feel defaults), if any.
fn system_value(file: &str, groups: &[&str], key: &str) -> Option<String> {
    let empty = std::env::temp_dir().join(format!("lumen-empty-config-{}", std::process::id()));
    std::fs::create_dir_all(&empty).ok()?;
    let mut args = vec!["--file", file];
    for g in groups {
        args.extend(["--group", g]);
    }
    args.extend(["--key", key]);
    let value = Command::new("kreadconfig6").args(&args).env("XDG_CONFIG_HOME", &empty).output();
    let _ = std::fs::remove_dir(&empty);
    value.ok().map(|o| String::from_utf8_lossy(&o.stdout).trim().to_owned()).filter(|v| !v.is_empty())
}

/// Back to the default: deleting a key that a lower config file sets would write `key[$d]` and mask it.
fn reset_key(file: &str, groups: &[&str], key: &str) -> Result<()> {
    match system_value(file, groups, key) {
        Some(v) => kwrite(file, groups, key, &v),
        None => kdelete(file, groups, key),
    }
}

/// `allow`: false restores the user's lock screen wallpaper whatever lumenrc says.
fn lock_wallpaper(allow: bool) -> Result<()> {
    let rc = crate::layout::Ini::parse(&std::fs::read_to_string(config_home().join("lumenrc")).unwrap_or_default());
    let lock = crate::layout::Ini::parse(&std::fs::read_to_string(config_home().join("kscreenlockerrc")).unwrap_or_default());
    let wp = ["Greeter", "Wallpaper", "org.kde.image", "General"];
    let backup = ["LockScreenWallpaper"];
    let read = |file: &str, groups: &[&str], key: &str| {
        let mut args = vec!["--file", file];
        for group in groups {
            args.extend(["--group", group]);
        }
        args.extend(["--key", key]);
        output("kreadconfig6", &args).unwrap_or_default()
    };
    if allow && rc.get(&["LockScreen"], "LumenWallpaper") == Some("true") {
        let Some(dir) = [data_home().join("wallpapers/Lumen"), PathBuf::from("/usr/share/wallpapers/Lumen")].into_iter().find(|p| p.is_dir()) else {
            return Ok(());
        };
        if rc.get(&backup, "Saved") != Some("true") {
            for (groups, key, present) in [(&["Greeter"][..], "WallpaperPlugin", "PluginPresent"), (&wp[..], "Image", "ImagePresent"), (&wp[..], "PreviewImage", "PreviewImagePresent")] {
                kwrite("lumenrc", &backup, present, if lock.get(groups, key).is_some() { "true" } else { "false" })?;
                kwrite("lumenrc", &backup, key, &read("kscreenlockerrc", groups, key))?;
            }
            kwrite("lumenrc", &backup, "Saved", "true")?;
        }
        kwrite("kscreenlockerrc", &["Greeter"], "WallpaperPlugin", "org.kde.image")?;
        let url = format!("file://{}", dir.display());
        for key in ["Image", "PreviewImage"] {
            kwrite("kscreenlockerrc", &wp, key, &url)?;
        }
    } else {
        if read("kscreenlockerrc", &wp, "Image").contains("/wallpapers/Lumen") {
            for key in ["Image", "PreviewImage"] {
                if rc.get(&backup, &format!("{key}Present")) == Some("true") {
                    kwrite("kscreenlockerrc", &wp, key, &read("lumenrc", &backup, key))?;
                } else {
                    kdelete("kscreenlockerrc", &wp, key)?;
                }
            }
            if rc.get(&backup, "Saved") == Some("true") && read("kscreenlockerrc", &["Greeter"], "WallpaperPlugin") == "org.kde.image" {
                if rc.get(&backup, "PluginPresent") == Some("true") {
                    kwrite("kscreenlockerrc", &["Greeter"], "WallpaperPlugin", &read("lumenrc", &backup, "WallpaperPlugin"))?;
                } else {
                    kdelete("kscreenlockerrc", &["Greeter"], "WallpaperPlugin")?;
                }
            }
        }
        for key in ["Saved", "PluginPresent", "WallpaperPlugin", "ImagePresent", "Image", "PreviewImagePresent", "PreviewImage"] {
            kdelete("lumenrc", &backup, key)?;
        }
    }
    Ok(())
}

pub fn build_native(ctx: &Ctx) -> Result<()> {
    build_native_in(ctx, &ctx.root.join("build/cmake"))
}

fn build_native_in(ctx: &Ctx, dir: &Path) -> Result<()> {
    let root = ctx.root.to_string_lossy().into_owned();
    let dir_s = dir.to_string_lossy().into_owned();
    let gen_dir = format!("-DLUMEN_GEN_DIR={}", ctx.gen_dir.join("cpp").display());
    run(
        "cmake",
        &["-S", &root, "-B", &dir_s, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=/usr", "-DBUILD_TESTING=OFF", &gen_dir],
    )?;
    run("cmake", &["--build", &dir_s, "--parallel"])
}

/// Copy build/out/<from> into <to>, returning installed paths.
fn copy_dir(from: &Path, to: &Path, files: &mut Vec<PathBuf>) -> Result<()> {
    if !from.is_dir() {
        return Ok(());
    }
    for e in walkdir::WalkDir::new(from).into_iter().filter_map(|e| e.ok()).filter(|e| e.file_type().is_file()) {
        let rel = e.path().strip_prefix(from)?;
        let dst = to.join(rel);
        write_file(&dst, &std::fs::read(e.path())?)?;
        files.push(dst);
    }
    Ok(())
}

fn manifest_path() -> PathBuf {
    data_home().join("lumen/manifest")
}

fn remove_manifest(path: &Path) -> Result<()> {
    let Ok(list) = std::fs::read_to_string(path) else { return Ok(()) };
    for f in list.lines().filter(|l| !l.is_empty()) {
        let _ = std::fs::remove_file(f);
    }
    for f in list.lines().filter(|l| !l.is_empty()) {
        let mut p = Path::new(f).parent();
        while let Some(dir) = p {
            if std::fs::remove_dir(dir).is_err() {
                break;
            }
            p = dir.parent();
        }
    }
    Ok(())
}

pub fn install_user(ctx: &Ctx) -> Result<()> {
    let manifest = manifest_path();
    remove_manifest(&manifest)?;
    let mut files = Vec::new();
    copy_dir(&ctx.out_path("share"), &data_home(), &mut files)?;
    copy_dir(&ctx.out_path("config/gtk-4.0"), &data_home().join("lumen/gtk-4.0"), &mut files)?;
    write_file(&data_home().join("lumen/root"), ctx.root.to_string_lossy().as_bytes())?;
    let list: String = files.iter().map(|p| format!("{}\n", p.display())).collect();
    write_file(&manifest, list.as_bytes())?;
    try_run("kbuildsycoca6", &["--noincremental"]);
    println!("installed {} user files", files.len());
    install_fonts(ctx, false)
}

/// Install locations of the Qt Quick Controls style and its Kirigami platform-plugin link.
fn plugin_dir() -> String {
    output("qtpaths6", &["--query", "QT_INSTALL_PLUGINS"]).unwrap_or_else(|| "/usr/lib/qt6/plugins".into())
}

fn qml_dir() -> String {
    output("qtpaths6", &["--query", "QT_INSTALL_QML"]).unwrap_or_else(|| "/usr/lib/qt6/qml".into())
}

fn qqc2_paths() -> (String, String) {
    (format!("{}/org/lumen/desktop", qml_dir()), format!("{}/kf6/kirigami/platform/org.lumen.desktop.so", plugin_dir()))
}

/// Rebuilds the Kirigami shadow tree from the installed Kirigami (also run by the pacman hook after Kirigami updates).
/// FormLayout's qmldir loses its `prefer` line so the patched file on disk wins over the compiled copy; if the patch no
/// longer applies the tree serves stock Kirigami.
fn kirigami_shadow(ctx: &Ctx) -> Result<()> {
    use std::os::unix::fs::symlink;
    let real = PathBuf::from(format!("{}/org/kde/kirigami", qml_dir()));
    // The pacman hook runs as root: stage outside the user's tree so later user builds can still write it.
    let stage = if is_root() { PathBuf::from("/var/cache/lumen/kirigami-shadow") } else { ctx.gen_dir.join("kirigami-shadow") };
    let _ = std::fs::remove_dir_all(&stage);
    let kirigami = stage.join("org/kde/kirigami");
    let layouts = kirigami.join("layouts");
    std::fs::create_dir_all(&layouts)?;
    let patched = layouts.join("FormLayout.qml");
    let ok = Command::new("patch")
        .args(["--silent", "--forward", "--reject-file=-", "-o"])
        .arg(&patched)
        .arg(real.join("layouts/FormLayout.qml"))
        .arg(ctx.root.join("kirigami-shadow/FormLayout.qml.patch"))
        .status()
        .is_ok_and(|s| s.success());
    for entry in std::fs::read_dir(&real)?.filter_map(|e| e.ok()) {
        if !ok || entry.file_name() != "layouts" {
            symlink(entry.path(), kirigami.join(entry.file_name()))?;
        }
    }
    if ok {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(&patched, std::fs::Permissions::from_mode(0o644))?;
        for entry in std::fs::read_dir(real.join("layouts"))?.filter_map(|e| e.ok()) {
            let name = entry.file_name();
            if name != "qmldir" && name != "FormLayout.qml" {
                symlink(entry.path(), layouts.join(name))?;
            }
        }
        let qmldir: String = std::fs::read_to_string(real.join("layouts/qmldir"))?.lines().filter(|l| !l.starts_with("prefer ")).map(|l| format!("{l}\n")).collect();
        write_file(&layouts.join("qmldir"), qmldir.as_bytes())?;
    } else {
        let _ = std::fs::remove_dir_all(&layouts);
        symlink(real.join("layouts"), &layouts)?;
        println!("FormLayout patch does not fit this Kirigami; settings forms stay stock");
    }
    root_cmd("rm", &["-rf", KIRIGAMI_SHADOW])?;
    root_cmd("mkdir", &["-p", KIRIGAMI_SHADOW])?;
    root_cmd("cp", &["-a", "--no-preserve=ownership", &format!("{}/.", stage.display()), KIRIGAMI_SHADOW])
}

/// Kirigami resolves styled components under its own install root, next to the stock styles.
fn kirigami_style_path() -> String {
    format!("{}/org/kde/kirigami/styles/{QQC2_STYLE}", qml_dir())
}

/// Plugins the running KWin keeps loaded, relative to the Qt plugin directory.
const LIVE_PLUGINS: [&str; 2] = ["kwin/effects/plugins/lumen_materials.so", "org.kde.kdecoration3/org.lumen.decoration.so"];
const DECORATION_LIBRARY: &str = "org.lumen.decoration";

/// Puts a rebuilt plugin into the running KWin without restarting it. KWin keeps the old file mapped and dlopen()
/// reuses it by path, so the plugin is loaded once from a fresh copy: KWin also searches its own directory (/usr/bin),
/// which briefly links there while the installed file is moved aside.
fn reload_live(plugins: &str, rel: &str) -> Result<()> {
    let (top, rest) = rel.split_once('/').context("plugin path")?;
    let effect = top == "kwin";
    let in_use = if effect {
        output("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.isEffectLoaded", "lumen_materials"]).is_some_and(|s| s == "true")
    } else {
        Path::new(&format!("{plugins}/{top}/org.kde.breeze.so")).is_file()
            && output("kreadconfig6", &["--file", "kwinrc", "--group", "org.kde.kdecoration2", "--key", "library"]).is_some_and(|s| s == DECORATION_LIBRARY)
    };
    let link = format!("/usr/bin/{top}");
    if effect && !in_use {
        // KWin refused the old build (another KWin version): the new one loads directly, unless the user turned it off.
        if output("kreadconfig6", &["--file", "kwinrc", "--group", "Plugins", "--key", "lumen_materialsEnabled"]).is_none_or(|s| s != "false") {
            try_run("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.loadEffect", "lumen_materials"]);
        }
        return Ok(());
    }
    if !in_use || Path::new(&link).symlink_metadata().is_ok() {
        return Ok(());
    }
    let stamp = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH)?.as_secs();
    let fresh = format!("/usr/lib/lumen-live-{stamp}");
    let installed = format!("{plugins}/{rel}");
    let aside = format!("{fresh}.aside");
    let parent = Path::new(rest).parent().map(|p| p.to_string_lossy().into_owned()).unwrap_or_default();
    root_cmd("mkdir", &["-p", &format!("{fresh}/{parent}")])?;
    root_cmd("cp", &[&installed, &format!("{fresh}/{rest}")])?;
    root_cmd("ln", &["-sfn", &fresh, &link])?;
    root_cmd("mv", &[&installed, &aside])?;
    let pause = |ms| std::thread::sleep(std::time::Duration::from_millis(ms));
    if effect {
        try_run("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.unloadEffect", "lumen_materials"]);
        pause(500);
        try_run("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.loadEffect", "lumen_materials"]);
    } else {
        for library in ["org.kde.breeze", DECORATION_LIBRARY] {
            try_run("kwriteconfig6", &["--file", "kwinrc", "--group", "org.kde.kdecoration2", "--key", "library", library]);
            try_run("qdbus6", &["org.kde.KWin", "/KWin", "reconfigure"]);
            pause(1000);
        }
    }
    pause(500);
    let restored = root_cmd("mv", &[&aside, &installed]);
    root_cmd("rm", &["-rf", &link, &fresh])?;
    restored?;
    if effect {
        try_run("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.loadEffect", "lumen_materials"]);
    }
    println!("reloaded {rel} in the running KWin");
    Ok(())
}

const QQC2_STYLE: &str = "org.lumen.desktop";
/// Kirigami forms as cards, like Windows Settings; System Settings forces flat forms unless asked for cards.
const FORMS_STYLE: &str = "KDE_KIRIGAMI_FORMS_STYLE=cards";

/// Session-wide Qt Quick Controls style, read by Plasma at login; set only while the style is installed so Qt Quick
/// apps never ask for a missing style.
fn qqc2_env(enable: bool) -> Result<()> {
    let file = config_home().join("plasma-workspace/env/lumen-qqc2.sh");
    if enable && Path::new(&qqc2_paths().0).join("qmldir").is_file() {
        let mut vars = vec![format!("QT_QUICK_CONTROLS_STYLE={QQC2_STYLE}"), FORMS_STYLE.to_owned()];
        if Path::new(KIRIGAMI_SHADOW).join("org/kde/kirigami/qmldir").exists() {
            vars.push(format!("QML_IMPORT_PATH={KIRIGAMI_SHADOW}"));
        }
        write_file(&file, vars.iter().map(|v| format!("export {v}\n")).collect::<String>().as_bytes())?;
        let mut args = vec!["--user", "set-environment"];
        args.extend(vars.iter().map(String::as_str));
        try_run("systemctl", &args);
    } else {
        let _ = std::fs::remove_file(&file);
        try_run("systemctl", &["--user", "unset-environment", "QT_QUICK_CONTROLS_STYLE", FORMS_STYLE.split('=').next().unwrap_or_default(), "QML_IMPORT_PATH"]);
    }
    Ok(())
}

pub fn install_system(ctx: &Ctx, no_boot: bool) -> Result<()> {
    let build = ctx.root.join("build/cmake").to_string_lossy().into_owned();
    let plugins = plugin_dir();
    let before: Vec<_> = LIVE_PLUGINS.iter().map(|rel| std::fs::read(format!("{plugins}/{rel}")).ok()).collect();
    root_cmd("cmake", &["--install", &build])?;
    install_fonts(ctx, true)?;
    for (rel, old) in LIVE_PLUGINS.iter().zip(before) {
        if std::fs::read(format!("{plugins}/{rel}")).ok() != old {
            reload_live(&plugins, rel)?;
        }
    }
    if Path::new(DOLPHIN_BIN).is_file() {
        let tmp = ctx.gen_dir.join("plasma-dolphin-lumen.conf");
        write_file(&tmp, format!("[Service]\nExecStart=\nExecStart={DOLPHIN_BIN} --daemon\n").as_bytes())?;
        root_cmd("install", &["-Dm644", &tmp.to_string_lossy(), DOLPHIN_DROPIN])?;
        try_run("systemctl", &["--user", "daemon-reload"]);
    }

    let out = |p: &str| ctx.out_path(p).to_string_lossy().into_owned();
    let share_dirs = [
        ("share/color-schemes", "/usr/share/color-schemes"),
        ("share/plasma/desktoptheme", "/usr/share/plasma/desktoptheme"),
        ("share/plasma/look-and-feel", "/usr/share/plasma/look-and-feel"),
        ("share/wallpapers", "/usr/share/wallpapers"),
        ("share/themes", "/usr/share/themes"),
        ("system/plymouth/themes", "/usr/share/plymouth/themes"),
    ];
    for (src, dst) in share_dirs {
        if ctx.out_path(src).is_dir() {
            root_cmd("cp", &["-rT", &out(src), dst])?;
        }
    }
    if ctx.out_path(QQC2_OUT).is_dir() {
        let (qml, kirigami) = qqc2_paths();
        root_cmd("rm", &["-rf", &qml])?;
        root_cmd("mkdir", &["-p", &qml])?;
        root_cmd("cp", &["-rT", &out(QQC2_OUT), &qml])?;
        // Kirigami loads its platform theme by a plugin file name containing the style name.
        root_cmd("ln", &["-sfn", "org.kde.desktop.so", &kirigami])?;
    }
    if ctx.out_path(KIRIGAMI_STYLE_OUT).is_dir() {
        let dst = kirigami_style_path();
        root_cmd("rm", &["-rf", &dst])?;
        root_cmd("mkdir", &["-p", &dst])?;
        root_cmd("cp", &["-rT", &out(KIRIGAMI_STYLE_OUT), &dst])?;
    }
    root_cmd("install", &["-Dm644", &out("system/vconsole/vtrgb"), &format!("{SYS_STATE}/vtrgb")])?;
    root_cmd("install", &["-Dm644", &out("system/systemd/lumen-vtrgb.service"), "/etc/systemd/system/lumen-vtrgb.service"])?;
    root_cmd("systemctl", &["enable", "lumen-vtrgb.service"])?;

    let bin = ctx.root.join("tools/lumen/target/release/lumen");
    if bin.is_file() {
        root_cmd("install", &["-Dm755", &bin.to_string_lossy(), "/usr/local/bin/lumen"])?;
    }
    let linked = linked_packages(&ctx.root.join("build/cmake"));
    let linked: Vec<&str> = linked.iter().map(String::as_str).collect();
    hook(ctx, KWIN_HOOK, &linked, "Rebuilding Lumen native components for the new Qt/KDE libraries...", "rebuild-kwin")?;
    hook(ctx, SHELL_HOOK, SHELL_TARGETS, "Refreshing the Lumen lock and logout screens and Kirigami forms...", "refresh-shell")?;

    refresh_shell(ctx)?;
    if !no_boot {
        boot(ctx, true)?;
    }
    login(ctx, true)?;
    println!("system install done");
    Ok(())
}

fn hook_text(targets: &[&str], what: &str, action: &str) -> String {
    let targets: String = targets.iter().map(|t| format!("Target = {t}\n")).collect();
    format!("[Trigger]\nOperation = Install\nOperation = Upgrade\nType = Package\n{targets}\n[Action]\nDescription = {what}\nWhen = PostTransaction\n{action}")
}

fn hook(ctx: &Ctx, path: &str, targets: &[&str], what: &str, cmd: &str) -> Result<()> {
    let text = hook_text(targets, what, &format!("Depends = cmake\nDepends = ninja\nExec = /usr/local/bin/lumen --root {} {cmd}\n", ctx.root.display()));
    let tmp = ctx.gen_dir.join(Path::new(path).file_name().unwrap());
    write_file(&tmp, text.as_bytes())?;
    root_cmd("install", &["-Dm644", &tmp.to_string_lossy(), path])
}

/// Packaged install: the system files land under `dest` (pacman's $pkgdir) and nothing else changes. The package's
/// hooks and `lumen` commands make the runtime state (shell copy, Kirigami shadow, boot and login themes).
pub fn stage(ctx: &Ctx, dest: &Path) -> Result<()> {
    use std::os::unix::fs::PermissionsExt;
    let at = |p: &str| dest.join(p.trim_start_matches('/'));
    let st = Command::new("cmake").arg("--install").arg(ctx.root.join("build/cmake")).env("DESTDIR", dest).status().context("spawn cmake")?;
    if !st.success() {
        bail!("cmake --install failed: {st}");
    }
    let mut files = Vec::new();
    copy_dir(&ctx.out_path("share"), &at("/usr/share"), &mut files)?;
    copy_dir(&ctx.out_path("system/plymouth/themes"), &at("/usr/share/plymouth/themes"), &mut files)?;
    let (qml, kirigami) = qqc2_paths();
    copy_dir(&ctx.out_path(QQC2_OUT), &at(&qml), &mut files)?;
    std::fs::create_dir_all(at(&kirigami).parent().context("kirigami plugin dir")?)?;
    let _ = std::fs::remove_file(at(&kirigami));
    std::os::unix::fs::symlink("org.kde.desktop.so", at(&kirigami))?;
    copy_dir(&ctx.out_path(KIRIGAMI_STYLE_OUT), &at(&kirigami_style_path()), &mut files)?;
    write_file(&at(&format!("{SYS_STATE}/vtrgb")), &std::fs::read(ctx.out_path("system/vconsole/vtrgb"))?)?;
    write_file(&at("/usr/lib/systemd/system/lumen-vtrgb.service"), &std::fs::read(ctx.out_path("system/systemd/lumen-vtrgb.service"))?)?;

    // What the installed CLI reads at run time (see ctx::PACKAGED_ROOT).
    let data = at(crate::ctx::PACKAGED_ROOT);
    for rel in ["tokens/tokens.toml", "kirigami-shadow/FormLayout.qml.patch", &format!("fonts/fontconfig/{FONT_CONF}")] {
        write_file(&data.join(rel), &std::fs::read(ctx.root.join(rel)).with_context(|| format!("read {rel}"))?)?;
    }
    for rel in ["build/out/system", "build/out/config", "build/out/apps"] {
        copy_dir(&ctx.root.join(rel), &data.join(rel), &mut files)?;
    }
    let bin = at("/usr/bin/lumen");
    write_file(&bin, &std::fs::read(ctx.root.join("tools/lumen/target/release/lumen")).context("lumen binary: run `cargo build --release` first")?)?;
    std::fs::set_permissions(&bin, std::fs::Permissions::from_mode(0o755))?;

    if at(PACKAGED_DOLPHIN).is_file() {
        write_file(&at(PACKAGED_DOLPHIN_DROPIN), format!("[Service]\nExecStart=\nExecStart={PACKAGED_DOLPHIN} --daemon\n").as_bytes())?;
        let bin_dir = Path::new(PACKAGED_DOLPHIN).parent().context("dolphin dir")?.display().to_string();
        let env = format!(
            "# Lumen's File Explorer (Dolphin fork) and its launcher come first in the Plasma session.\n\
             case \":$PATH:\" in *:{bin_dir}:*) ;; *) export PATH=\"{bin_dir}:$PATH\" ;; esac\n\
             case \":${{XDG_DATA_DIRS:=/usr/local/share:/usr/share}}:\" in *:{PACKAGED_XDG}:*) ;; *) export XDG_DATA_DIRS=\"{PACKAGED_XDG}:$XDG_DATA_DIRS\" ;; esac\n"
        );
        write_file(&at(SESSION_ENV), env.as_bytes())?;
    }
    let with_self = |targets: &[&'static str]| [targets, PACKAGES].concat();
    let shell = hook_text(&with_self(SHELL_TARGETS), "Refreshing the Lumen lock and logout screens and Kirigami forms...", "Exec = /usr/bin/lumen refresh-shell\n");
    write_file(&at(&format!("{ALPM_HOOKS}/lumen-shell.hook")), shell.as_bytes())?;
    let rebuild = hook_text(&with_self(REBUILD_TARGETS), "Checking Lumen's native plugins against the updated KDE/Qt...", "NeedsTargets\nExec = /usr/bin/lumen rebuild-kwin\n");
    write_file(&at(&format!("{ALPM_HOOKS}/lumen-rebuild.hook")), rebuild.as_bytes())?;
    // After lumen-shell.hook (hooks run in name order): offer or re-apply Lumen in running sessions once the shell
    // copy exists.
    let welcome = hook_text(PACKAGES, "Applying Lumen in running Plasma sessions...", "Exec = /usr/bin/lumen post-install\n");
    write_file(&at(&format!("{ALPM_HOOKS}/lumen-welcome.hook")), welcome.as_bytes())?;
    write_file(
        &at("/etc/xdg/autostart/org.lumen.welcome.desktop"),
        b"[Desktop Entry]\nType=Application\nName=Lumen\nExec=/usr/bin/lumen welcome\nOnlyShowIn=KDE;\nNoDisplay=true\nX-KDE-autostart-phase=2\n",
    )?;
    println!("staged {} generated files and the native build into {}", files.len(), dest.display());
    Ok(())
}

/// pacman pre_remove (root): undoes the runtime state Lumen made outside the package's own files.
pub fn pre_remove(ctx: &Ctx) -> Result<()> {
    boot(ctx, false)?;
    login(ctx, false)?;
    let fonts = Path::new(WIN_FONTS).is_dir();
    root_cmd(
        "rm",
        &["-rf", SHELL_DST, KIRIGAMI_SHADOW, GREETER_KWIN_DROPIN, "/var/cache/lumen", &format!("{SYS_STATE}/prev-plymouth"), WIN_FONTS, &format!("/etc/fonts/conf.d/{FONT_CONF}")],
    )?;
    if fonts {
        try_run("fc-cache", &["-f"]);
    }
    Ok(())
}

/// Rebuild the shell-package copy from the current stock package plus the Lumen overlay.
pub fn refresh_shell(ctx: &Ctx) -> Result<()> {
    kirigami_shadow(ctx)?;
    if Path::new("/etc/plasmalogin.conf").exists() {
        greeter_input_method(ctx)?;
    }
    let overlay = ctx.out_path("system/shell-overlay");
    if !overlay.is_dir() || !Path::new(SHELL_SRC).is_dir() {
        return Ok(());
    }
    root_cmd("rm", &["-rf", SHELL_DST])?;
    root_cmd("mkdir", &["-p", Path::new(SHELL_DST).parent().unwrap().to_str().unwrap()])?;
    root_cmd("cp", &["-a", SHELL_SRC, SHELL_DST])?;
    root_cmd("cp", &["-rT", &overlay.to_string_lossy(), SHELL_DST])?;
    applet_popup_gap(ctx)
}

/// Panel popups float `flyout_pad` above the taskbar, as Windows flyouts do; stock Plasma only spaces them from
/// floating panels. Skipped (stock file kept) when Plasma's line changes.
fn applet_popup_gap(ctx: &Ctx) -> Result<()> {
    const STOCK: &str = "? Kirigami.Units.largeSpacing : 0";
    // Applets may anchor their popup elsewhere (Start centres on the screen).
    const ANCHOR: &str = "visualParent: root.compactRepresentation";
    let Ok(qml) = std::fs::read_to_string(Path::new(SHELL_SRC).join("contents/applet/CompactApplet.qml")) else { return Ok(()) };
    if qml.matches(STOCK).count() != 1 || qml.matches(ANCHOR).count() != 1 {
        return Ok(());
    }
    let qml = qml
        .replace(STOCK, &format!("? Kirigami.Units.largeSpacing : {}", crate::tokens::fmt_num(ctx.t.f("size.flyout_pad"))))
        .replace(ANCHOR, "visualParent: root.plasmoidItem?.popupAnchor ?? root.compactRepresentation");
    let tmp = staged(ctx, "CompactApplet.qml", qml.as_bytes())?;
    root_cmd("install", &["-Dm644", &tmp.to_string_lossy(), &format!("{SHELL_DST}/contents/applet/CompactApplet.qml")])
}

/// A file for root_cmd to install; the pacman hook runs as root, so it stages outside the user's tree.
fn staged(ctx: &Ctx, name: &str, data: &[u8]) -> Result<PathBuf> {
    let tmp = if is_root() { PathBuf::from("/var/cache/lumen").join(name) } else { ctx.gen_dir.join(name) };
    write_file(&tmp, data)?;
    Ok(tmp)
}

/// The greeter's KWin asks for plasma-keyboard even when it is not installed, then keeps retrying to show it and the
/// login screen flickers until the password field is clicked: drop the flag while the keyboard is missing.
fn greeter_input_method(ctx: &Ctx) -> Result<()> {
    let exec = std::fs::read_to_string(GREETER_KWIN_UNIT).ok().and_then(|u| u.lines().find_map(|l| l.strip_prefix("ExecStart=").map(str::to_owned)));
    let Some(exec) = exec.filter(|e| e.contains("--inputmethod") && !Path::new("/usr/bin/plasma-keyboard").exists()) else {
        return root_cmd("rm", &["-f", GREETER_KWIN_DROPIN]);
    };
    let mut args = exec.split_whitespace();
    let mut kept = Vec::new();
    while let Some(a) = args.next() {
        match a {
            "--inputmethod" => {
                args.next();
            }
            _ if a.starts_with("--inputmethod=") => {}
            _ => kept.push(a),
        }
    }
    let tmp = staged(ctx, "plasma-login-kwin-lumen.conf", format!("[Service]\nExecStart=\nExecStart={}\n", kept.join(" ")).as_bytes())?;
    root_cmd("install", &["-Dm644", &tmp.to_string_lossy(), GREETER_KWIN_DROPIN])
}

fn child_ci(dir: &Path, name: &str) -> Option<PathBuf> {
    std::fs::read_dir(dir).ok()?.flatten().map(|e| e.path()).find(|p| p.file_name().is_some_and(|n| n.to_string_lossy().eq_ignore_ascii_case(name)))
}

/// Windows fonts kept in the repo (`lumen fonts`); git-ignored: Microsoft's licence forbids redistributing them.
fn repo_fonts(ctx: &Ctx) -> PathBuf {
    if ctx.packaged { data_home().join("lumen/windows-fonts") } else { ctx.root.join("fonts/windows") }
}

pub fn import_fonts(ctx: &Ctx, path: &Path) -> Result<()> {
    let dir = [&["Windows", "Fonts"][..], &["Fonts"], &[]]
        .iter()
        .filter_map(|parts| parts.iter().try_fold(path.to_path_buf(), |d, p| child_ci(&d, p)))
        .find(|d| child_ci(d, "segoeui.ttf").is_some())
        .with_context(|| format!("no Windows fonts (segoeui.ttf) under {}", path.display()))?;
    let dst = repo_fonts(ctx);
    std::fs::create_dir_all(&dst)?;
    let mut n = 0;
    for src in WIN_FONT_FILES.iter().filter_map(|f| child_ci(&dir, f)) {
        std::fs::copy(&src, dst.join(src.file_name().expect("file name"))).with_context(|| format!("copy {}", src.display()))?;
        n += 1;
    }
    println!("{n} fonts copied to {}", dst.display());
    install_fonts(ctx, true)
}

/// Installs the repo's Windows fonts system-wide with `system`, else for this user unless already system-wide.
fn install_fonts(ctx: &Ctx, system: bool) -> Result<()> {
    let user = data_home().join("fonts/lumen-windows");
    let files: Vec<String> = std::fs::read_dir(repo_fonts(ctx))
        .into_iter()
        .flatten()
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|e| e.eq_ignore_ascii_case("ttf")))
        .map(|p| p.to_string_lossy().into_owned())
        .collect();
    if files.is_empty() {
        return Ok(());
    }
    let target = if system {
        let _ = std::fs::remove_dir_all(&user);
        WIN_FONTS.to_owned()
    } else if Path::new(WIN_FONTS).is_dir() {
        return Ok(());
    } else {
        user.to_string_lossy().into_owned()
    };
    let mut args = vec!["-Dm644", "-t", target.as_str()];
    args.extend(files.iter().map(String::as_str));
    let conf = ctx.root.join("fonts/fontconfig").join(FONT_CONF).to_string_lossy().into_owned();
    if system {
        let _ = std::fs::remove_file(user_font_conf());
        root_cmd("install", &args)?;
        root_cmd("install", &["-Dm644", &conf, &format!("/etc/fonts/conf.d/{FONT_CONF}")])?;
        root_cmd("fc-cache", &["-f", &target])?;
    } else {
        run("install", &args)?;
        run("install", &["-Dm644", &conf, &user_font_conf().to_string_lossy()])?;
        run("fc-cache", &["-f", &target])?;
    }
    println!("{} Windows fonts installed to {target}", files.len());
    Ok(())
}

/// Gives Segoe UI and Consolas their own TrueType hinting: glyphs land on Windows' pixel grid (9 px caps at 12 px).
const FONT_CONF: &str = "61-lumen-windows.conf";

fn user_font_conf() -> PathBuf {
    config_home().join("fontconfig/conf.d").join(FONT_CONF)
}

fn has_font(family: &str) -> bool {
    output("fc-list", &[family, "family"]).is_some_and(|s| !s.is_empty())
}

/// Picks up icon, font and theme changes made outside Lumen: running apps reload on KDE's broadcasts,
/// the shell parts that cache artwork restart.
pub fn restart() -> Result<()> {
    try_run("kbuildsycoca6", &[]);
    fix_icon_colors()?;
    refresh_icons(true);
    println!("restarting the desktop shell");
    if !try_run("systemctl", &["--user", "restart", "plasma-plasmashell.service"]) {
        try_run("kquitapp6", &["plasmashell"]);
        try_run("kstart", &["plasmashell"]);
    }
    try_run("systemctl", &["--user", "try-restart", "plasma-xdg-desktop-portal-kde.service"]);
    if output("pidof", &["krunner"]).is_some() {
        try_run("kquitapp6", &["krunner"]);
    }
    try_run("dbus-send", &["--session", "--dest=org.kde.KWin", "--type=method_call", "/KWin", "org.kde.KWin.reconfigure"]);
    println!("done: apps refresh their icons, the taskbar and desktop are reloaded");
    Ok(())
}

/// Drops KDE's icon caches (and the Plasma theme caches) and tells running apps to reload their icons.
fn refresh_icons(plasma_caches: bool) {
    let cache = std::env::var_os("XDG_CACHE_HOME").map(PathBuf::from).unwrap_or_else(|| home().join(".cache"));
    for entry in std::fs::read_dir(&cache).into_iter().flatten().flatten() {
        let name = entry.file_name();
        let name = name.to_string_lossy();
        // ksvg-elements keeps element sizes and frame margins across theme changes.
        let plasma = plasma_caches && ((name.starts_with("plasma_theme_") && name.ends_with(".kcache")) || name == "ksvg-elements");
        if name == "icon-cache.kcache" || plasma {
            let _ = std::fs::remove_file(entry.path());
        }
    }
    // KIconLoader reloads the icon theme; KGlobalSettings IconChanged (4) refreshes the platform theme's icon theme name.
    try_run("dbus-send", &["--session", "--type=signal", "/KIconLoader", "org.kde.KIconLoader.iconChanged", "int32:0"]);
    try_run("dbus-send", &["--session", "--type=signal", "/KGlobalSettings", "org.kde.KGlobalSettings.notifyChange", "int32:4", "int32:0"]);
}

const KDE_SCHEME: &str = "id=\"current-color-scheme\"";
const KEPT_SCHEME: &str = "id=\"lumen-kept-color-scheme\"";

/// Some icon themes (Win11, Windows-Eleven, McMojave) give colored icons such as folder-red KDE's text-color class,
/// so KIconLoader repaints them in the text color. Renaming the stylesheet id keeps the author's color. Only the
/// active theme and the themes it inherits from, when installed in the user's data dir. Returns the files changed.
fn fix_icon_colors() -> Result<usize> {
    let root = data_home().join("icons");
    let mut queue = vec![output("kreadconfig6", &["--file", "kdeglobals", "--group", "Icons", "--key", "Theme"]).unwrap_or_default()];
    let mut seen = std::collections::HashSet::new();
    let mut fixed = 0;
    while let Some(name) = queue.pop() {
        if name.is_empty() || !seen.insert(name.clone()) {
            continue;
        }
        let dir = root.join(&name);
        let Ok(index) = std::fs::read_to_string(dir.join("index.theme")) else {
            continue;
        };
        if let Some(parents) = index.lines().find_map(|l| l.strip_prefix("Inherits=")) {
            queue.extend(parents.split(',').map(|p| p.trim().to_string()));
        }
        for entry in walkdir::WalkDir::new(&dir).into_iter().flatten() {
            if !entry.file_type().is_file() || entry.path().extension().is_none_or(|e| e != "svg") {
                continue;
            }
            let Ok(svg) = std::fs::read_to_string(entry.path()) else {
                continue;
            };
            if svg.contains(KDE_SCHEME) && svg.contains("class=\"ColorScheme-Text\"") && text_color_is_hue(&svg) {
                std::fs::write(entry.path(), svg.replacen(KDE_SCHEME, KEPT_SCHEME, 1))?;
                fixed += 1;
            }
        }
    }
    if fixed > 0 {
        println!("kept the colors of {fixed} icons that the icon theme marked as text-colored");
    }
    Ok(fixed)
}

/// True when the stylesheet's ColorScheme-Text color is a clear hue (red, blue...), not a gray meant to follow the text.
fn text_color_is_hue(svg: &str) -> bool {
    let Some(rule) = svg.find(".ColorScheme-Text").map(|i| &svg[i..]) else {
        return false;
    };
    let Some(hex) = rule.find("color:").and_then(|i| rule[i + 6..].trim_start().strip_prefix('#')) else {
        return false;
    };
    let channel = |i: usize| hex.get(i..i + 2).and_then(|c| u8::from_str_radix(c, 16).ok());
    match (channel(0), channel(2), channel(4)) {
        (Some(r), Some(g), Some(b)) => r.max(g).max(b) - r.min(g).min(b) > 40,
        _ => false,
    }
}

/// Undoes fix_icon_colors() in every user-installed icon theme.
fn restore_icon_colors() {
    for entry in walkdir::WalkDir::new(data_home().join("icons")).into_iter().flatten() {
        if entry.file_type().is_file()
            && entry.path().extension().is_some_and(|e| e == "svg")
            && let Ok(svg) = std::fs::read_to_string(entry.path())
            && svg.contains(KEPT_SCHEME)
        {
            let _ = std::fs::write(entry.path(), svg.replacen(KEPT_SCHEME, KDE_SCHEME, 1));
        }
    }
}

pub fn boot(ctx: &Ctx, on: bool) -> Result<()> {
    let vtrgb = Path::new("/etc/systemd/system/lumen-vtrgb.service").is_file() || Path::new("/usr/lib/systemd/system/lumen-vtrgb.service").is_file();
    if on {
        if vtrgb {
            root_cmd("systemctl", &["enable", "lumen-vtrgb.service"])?;
        }
        if output("plymouth-set-default-theme", &[]).is_some() {
            plymouth(ctx)?;
        }
        return limine(ctx);
    }
    if vtrgb {
        let _ = root_cmd("systemctl", &["disable", "lumen-vtrgb.service"]);
    }
    if output("plymouth-set-default-theme", &[]).as_deref() == Some("lumen") {
        let prev = std::fs::read_to_string(format!("{SYS_STATE}/prev-plymouth")).unwrap_or_else(|_| "bgrt".into());
        root_cmd("plymouth-set-default-theme", &["-R", prev.trim()])?;
    }
    let esp = esp_path();
    let backup = format!("{esp}/limine.conf.lumen-backup");
    if root_exists(&backup) {
        root_cmd("mv", &[&backup, &format!("{esp}/limine.conf")])?;
    }
    if root_exists(&format!("{esp}/lumen.png")) {
        root_cmd("rm", &["-f", &format!("{esp}/lumen.png")])?;
    }
    Ok(())
}

pub fn login(ctx: &Ctx, on: bool) -> Result<()> {
    if !Path::new("/etc/plasmalogin.conf").exists() {
        return Ok(());
    }
    if on {
        greeter_input_method(ctx)?;
        return login_screen();
    }
    root_cmd("rm", &["-f", GREETER_KWIN_DROPIN])?;
    let kg = format!("{PLM_HOME}/.config/kdeglobals");
    let greeter = root_read(&kg).unwrap_or_default();
    if !greeter.lines().any(|l| l.starts_with("LookAndFeelPackage=org.lumen.")) {
        return Ok(());
    }
    for (group, key) in [("General", "ColorScheme"), ("KDE", "widgetStyle"), ("KDE", "LookAndFeelPackage")] {
        root_cmd("kwriteconfig6", &["--file", &kg, "--group", group, "--key", key, "--delete"])?;
    }
    root_cmd("kwriteconfig6", &["--file", &format!("{PLM_HOME}/.config/plasmarc"), "--group", "Theme", "--key", "name", "--delete"])?;
    Ok(())
}

fn plymouth(_ctx: &Ctx) -> Result<()> {
    if let Some(prev) = output("plymouth-set-default-theme", &[])
        && prev != "lumen"
    {
        let tmp = std::env::temp_dir().join("lumen-prev-plymouth");
        write_file(&tmp, prev.as_bytes())?;
        root_cmd("install", &["-Dm644", &tmp.to_string_lossy(), &format!("{SYS_STATE}/prev-plymouth")])?;
    }
    root_cmd("plymouth-set-default-theme", &["-R", "lumen"])
}

fn esp_path() -> String {
    std::fs::read_to_string("/etc/default/limine")
        .ok()
        .and_then(|s| s.lines().find_map(|l| l.strip_prefix("ESP_PATH=").map(|v| v.trim_matches('"').to_owned())))
        .unwrap_or_else(|| "/boot".into())
}

fn limine(ctx: &Ctx) -> Result<()> {
    let esp = esp_path();
    let conf = format!("{esp}/limine.conf");
    let Some(current) = root_read(&conf) else {
        println!("limine: {conf} not readable, skipped");
        return Ok(());
    };
    let theme = std::fs::read_to_string(ctx.out_path("system/limine/theme.conf"))?;
    let theme_keys: Vec<&str> = theme.lines().filter_map(|l| l.split_once(':').map(|(k, _)| k.trim())).filter(|k| !k.starts_with('#')).collect();
    let mut kept = Vec::new();
    let mut in_block = false;
    for l in current.lines() {
        match l.trim() {
            "# lumen-begin (managed by `lumen install --system`)" => in_block = true,
            "# lumen-end" => in_block = false,
            _ if in_block => {}
            t if !l.starts_with(' ') && theme_keys.iter().any(|k| t.starts_with(&format!("{k}:"))) => {}
            _ => kept.push(l),
        }
    }
    let merged = format!("{theme}\n{}\n", kept.join("\n").trim_start());
    let tmp = ctx.gen_dir.join("limine.conf");
    write_file(&tmp, merged.as_bytes())?;
    let backup = format!("{conf}.lumen-backup");
    if !root_exists(&backup) {
        root_cmd("cp", &[&conf, &backup])?;
    }
    root_cmd("install", &["-m644", &tmp.to_string_lossy(), &conf])?;
    root_cmd("install", &["-m644", &ctx.out_path("system/limine/lumen.png").to_string_lossy(), &format!("{esp}/lumen.png")])
}

fn login_screen() -> Result<()> {
    let kg = format!("{PLM_HOME}/.config/kdeglobals");
    for (group, key, val) in [
        ("General", "ColorScheme", "LumenDark"),
        ("KDE", "widgetStyle", "Lumen"),
        ("KDE", "LookAndFeelPackage", "org.lumen.desktop.dark"),
    ] {
        root_cmd("kwriteconfig6", &["--file", &kg, "--group", group, "--key", key, val])?;
    }
    root_cmd("kwriteconfig6", &["--file", &format!("{PLM_HOME}/.config/plasmarc"), "--group", "Theme", "--key", "name", "lumen-dark"])?;
    root_cmd("chown", &["-R", "plasmalogin:plasmalogin", &format!("{PLM_HOME}/.config")])
}

const TITLEBAR_RULE: &str = "lumen-titlebar";

/// KWin keeps only shadow, outline and corners for windows tagged "org.lumen.titlebar" (apps drawing their own title row).
fn titlebar_rule() -> Result<()> {
    let rules = output("kreadconfig6", &["--file", "kwinrulesrc", "--group", "General", "--key", "rules"]).unwrap_or_default();
    let mut ids: Vec<&str> = rules.split(',').filter(|s| !s.is_empty()).collect();
    if !ids.contains(&TITLEBAR_RULE) {
        ids.push(TITLEBAR_RULE);
        kwrite("kwinrulesrc", &["General"], "rules", &ids.join(","))?;
        kwrite("kwinrulesrc", &["General"], "count", &ids.len().to_string())?;
    }
    for (key, value) in [
        ("Description", "Lumen: apps that draw their own title row"),
        ("tag", "org.lumen.titlebar"),
        ("tagmatch", "1"),
        ("decorationpolicy", "shadow"),
        ("decorationpolicyrule", "2"),
    ] {
        kwrite("kwinrulesrc", &[TITLEBAR_RULE], key, value)?;
    }
    Ok(())
}

pub fn apply(ctx: &Ctx, mode: ModeArg, layout: bool) -> Result<()> {
    let t = &ctx.t;
    let auto = matches!(mode, ModeArg::Auto);
    let light = matches!(mode, ModeArg::Light)
        || auto && output("kreadconfig6", &["--file", "kdeglobals", "--group", "Colors:Window", "--key", "BackgroundNormal"])
            .is_some_and(|rgb| {
                let rgb: Vec<f64> = rgb.split(',').filter_map(|v| v.parse().ok()).collect();
                rgb.len() >= 3 && 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2] >= 128.0
            });
    let (lnf, variant) = if light { ("org.lumen.desktop.light", "light") } else { ("org.lumen.desktop.dark", "dark") };
    if !auto {
        kwrite_notify("kdeglobals", &["KDE"], "AutomaticLookAndFeel", "false")?;
    }
    let snapshot = if layout {
        let snap = crate::layout::snapshot(&crate::layout::appletsrc());
        crate::layout::write_lumenrc(&snap)?;
        Some(snap)
    } else {
        None
    };
    keep_user_themes(|| {
        if layout {
            run("plasma-apply-lookandfeel", &["--keep-auto", "--apply", lnf, "--resetLayout"])
        } else {
            run("plasma-apply-lookandfeel", &["--keep-auto", "--apply", lnf])
        }
    })?;
    if fix_icon_colors()? > 0 {
        refresh_icons(false);
    }
    if let Some(snap) = &snapshot
        && crate::layout::transfer_tray_children(snap)?
        && !try_run("systemctl", &["--user", "restart", "plasma-plasmashell.service"])
    {
        try_run("kquitapp6", &["plasmashell"]);
        try_run("kstart", &["plasmashell"]);
    }
    kwrite_notify("kdeglobals", &["KDE"], "DefaultDarkLookAndFeel", "org.lumen.desktop.dark")?;
    kwrite_notify("kdeglobals", &["KDE"], "DefaultLightLookAndFeel", "org.lumen.desktop.light")?;
    kwrite_notify("kdeglobals", &["KDE"], "AutomaticLookAndFeel", if auto { "true" } else { "false" })?;
    kwrite_notify("lumenrc", &["Appearance"], "Mode", if auto { "auto" } else { variant })?;
    // Windows buttons carry text only (OK, Cancel, …); the Lumen style and Breeze both honour this.
    kwrite("kdeglobals", &["KDE"], "ShowIconsOnPushButtons", "false")?;
    qqc2_env(true)?;

    let lumenrc = |group: &str, key: &str, default: &str| output("kreadconfig6", &["--file", "lumenrc", "--group", group, "--key", key, "--default", default]).unwrap_or_else(|| default.into());

    // Windows "Text size" (Accessibility): one percentage for all text.
    let default_size = t.i("font.text_size").to_string();
    let scale = lumenrc("Fonts", "TextSize", &default_size).parse::<f64>().unwrap_or(100.0).clamp(100.0, 225.0) / 100.0;
    let font = |family: &str, key: &str, weight: u32| format!("{family},{},-1,5,{weight},0,0,0,0,0,0,0,0,0,0,1", (t.f(key) * scale * 10.0).round() / 10.0);
    // Segoe UI (Variable) and Consolas only when installed, else the open fallbacks in token order.
    let ui = std::iter::once(t.s("font.ui").to_owned()).chain(t.sv("font.fallback")).find(|f| has_font(f)).unwrap_or_else(|| t.s("font.ui").to_owned());
    let or_ui = |key: &str| if has_font(t.s(key)) { t.s(key).to_owned() } else { ui.clone() };
    let mono = if has_font(t.s("font.win32_mono")) { t.s("font.win32_mono") } else { t.s("font.mono") };
    kwrite("kdeglobals", &["General"], "font", &font(&ui, "font.size_pt", 400))?;
    kwrite("kdeglobals", &["General"], "menuFont", &font(&ui, "font.menu_pt", 400))?;
    kwrite("kdeglobals", &["General"], "toolBarFont", &font(&ui, "font.size_pt", 400))?;
    kwrite("kdeglobals", &["General"], "smallestReadableFont", &font(&or_ui("font.ui_small"), "font.small_pt", 400))?;
    kwrite("kdeglobals", &["General"], "fixed", &font(mono, "font.mono_pt", 400))?;
    kwrite("kdeglobals", &["WM"], "activeFont", &font(&or_ui("font.win32"), "font.size_pt", t.i("font.title_weight") as u32))?;
    // Running apps re-read their fonts, as after a change in the Fonts module.
    try_run("dbus-send", &["--session", "--type=signal", "/KDEPlatformTheme", "org.kde.KDEPlatformTheme.refreshFonts"]);
    // Stock blur takes over when the materials effect is switched off in the Lumen settings.
    let materials = lumenrc("Materials", "Enabled", "true") == "true";
    kwrite("kwinrc", &["Plugins"], "lumen_materialsEnabled", if materials { "true" } else { "false" })?;
    kwrite("kwinrc", &["Plugins"], "blurEnabled", if materials { "false" } else { "true" })?;
    for (k, v) in [
        ("glassEnabled", "false"),
        ("contrastEnabled", "false"),
        ("blurplusEnabled", "false"),
        ("forceblurEnabled", "false"),
        ("better_blurEnabled", "false"),
        ("better_blur_dxEnabled", "false"),
        ("shapecornersEnabled", "false"),
        ("kwin4_effect_shapecornersEnabled", "false"),
        ("scaleEnabled", "false"),
        ("glideEnabled", "false"),
        ("squashEnabled", "false"),
        ("magiclampEnabled", "false"),
    ] {
        kwrite("kwinrc", &["Plugins"], k, v)?;
    }
    let window_open = lumenrc("Animations", "WindowOpen", "true") == "true";
    let minimize = lumenrc("Animations", "Minimize", "true") == "true";
    kwrite("kwinrc", &["Plugins"], "lumen_openEnabled", if window_open { "true" } else { "false" })?;
    kwrite("kwinrc", &["Plugins"], "lumen_minimizeEnabled", if minimize { "true" } else { "false" })?;
    let duration = lumenrc("Animations", "WindowDuration", "100").parse::<f64>().unwrap_or(100.0).clamp(25.0, 400.0) / 100.0;
    for (group, key, token) in [("Effect-lumen_open", "OpenDuration", "motion.normal_ms"), ("Effect-lumen_open", "CloseDuration", "motion.fast_ms"), ("Effect-lumen_minimize", "Duration", "motion.minimize_ms")] {
        kwrite("kwinrc", &[group], key, &((t.f(token) * duration) as i64).to_string())?;
    }
    let scale = lumenrc("Animations", "OpenScale", "92").parse::<f64>().unwrap_or(92.0).clamp(50.0, 100.0) / 100.0;
    kwrite("kwinrc", &["Effect-lumen_open"], "OpenScale", &scale.to_string())?;
    // User tuning (Lumen settings module) wins over token defaults.
    for (key, token) in [
        ("AcrylicStrength", "material.acrylic.strength"),
        ("AcrylicNoise", "material.acrylic.noise"),
        ("AcrylicSaturation", "material.acrylic.saturation"),
        ("MicaStrength", "material.mica.strength"),
        ("MicaSaturation", "material.mica.saturation"),
    ] {
        let value = lumenrc("Materials", key, &crate::tokens::fmt_num(t.f(token)));
        kwrite("kwinrc", &["Effect-lumen_materials"], key, &value)?;
    }
    kwrite("kwinrc", &["Effect-lumen_materials"], "WallpaperTone", &lumenrc("Materials", "WallpaperTone", &crate::tokens::fmt_num(t.f("material.mica.wallpaper_tone"))))?;
    kwrite("kwinrc", &["Effect-lumen_materials"], "MicaForPanels", &lumenrc("Materials", "MicaForPanels", "true"))?;
    kwrite("kwinrc", &["org.kde.kdecoration2"], "library", DECORATION_LIBRARY)?;
    titlebar_rule()?;
    kwrite("kwinrc", &["org.kde.kdecoration2"], "theme", "Lumen")?;
    let icon = if lumenrc("Decoration", "ShowIcon", "true") == "true" { "M" } else { "" };
    let (left, right) = match lumenrc("Decoration", "ButtonLayout", "0").as_str() {
        "1" => (icon, "X"),
        "2" => ("XIA", icon),
        _ => (icon, "IAX"),
    };
    kwrite("kwinrc", &["org.kde.kdecoration2"], "ButtonsOnLeft", left)?;
    kwrite("kwinrc", &["org.kde.kdecoration2"], "ButtonsOnRight", right)?;
    unpin_app_schemes();
    keep_other_notifications();

    lock_wallpaper(true)?;

    gtk(ctx, variant)?;
    adopt_lumen_widgets();
    adopt_lumen_notifier();
    if let Some(snap) = &snapshot {
        keep_layout_choices(snap)?;
    }
    apply_taskbar(t);

    try_run("qdbus6", &["org.kde.KWin", "/KWin", "reconfigure"]);
    for e in ["blur", "glass", "contrast", "forceblur", "better_blur", "better_blur_dx", "shapecorners", "kwin4_effect_shapecorners", "scale", "glide", "squash", "magiclamp"] {
        try_run("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.unloadEffect", e]);
    }
    for (e, enabled) in [("lumen_materials", materials), ("blur", !materials), ("lumen_open", window_open), ("lumen_minimize", minimize)] {
        let method = if enabled { "org.kde.kwin.Effects.loadEffect" } else { "org.kde.kwin.Effects.unloadEffect" };
        try_run("qdbus6", &["org.kde.KWin", "/Effects", method, e]);
        if enabled {
            try_run("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.reconfigureEffect", e]);
        }
    }
    first_apply()?;
    kwrite("lumenrc", &["Restore"], "AppliedVersion", ctx.version())?;
    if activate_file_manager() {
        // The shell and search launch apps with their own environment: restart them on the new one.
        if !try_run("systemctl", &["--user", "restart", "plasma-plasmashell.service"]) {
            try_run("kquitapp6", &["plasmashell"]);
            try_run("kstart", &["plasmashell"]);
        }
        try_run("kquitapp6", &["krunner"]);
    }
    println!("applied Lumen ({}), run `lumen doctor` to verify", if auto { "auto" } else { variant });
    Ok(())
}

const ICON_THEME: &str = "windows-modern";
/// Resolves to the Windows Start logo in windows-modern (its start-here-kde is the KDE logo).
const START_ICON: &str = "start-here";

/// First apply only: Lumen's icon theme (the previous one is kept for `lumen uninstall`) and the Start logo on Lumen's
/// Start buttons. Later applies leave both to the user.
fn first_apply() -> Result<()> {
    let restore = |key: &str| output("kreadconfig6", &["--file", "lumenrc", "--group", "Restore", "--key", key]).unwrap_or_default();
    if restore("FirstApplied") == "true" {
        return Ok(());
    }
    let installed = [data_home().join("icons"), PathBuf::from("/usr/share/icons")].iter().any(|d| d.join(ICON_THEME).join("index.theme").is_file());
    let current = output("kreadconfig6", &["--file", "kdeglobals", "--group", "Icons", "--key", "Theme"]).filter(|t| !t.is_empty()).unwrap_or_else(|| "breeze".into());
    if installed && current != ICON_THEME {
        kwrite("lumenrc", &["Restore"], "IconTheme", &current)?;
        set_icon_theme(ICON_THEME)?;
        println!("icon theme: {ICON_THEME} (was {current}; `lumen uninstall` restores it)");
    }
    let script = format!(
        "panels().forEach(function (p) {{ p.widgets('org.lumen.start').forEach(function (w) {{\
            w.currentConfigGroup = ['General']; w.writeConfig('icon', '{START_ICON}'); w.reloadConfig(); }}); }});"
    );
    try_run("qdbus6", &["org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", &script]);
    kwrite("lumenrc", &["Restore"], "FirstApplied", "true")
}

/// Live switch: running apps reload their icons.
fn set_icon_theme(theme: &str) -> Result<()> {
    if !try_run("/usr/lib/plasma-changeicons", &[theme]) {
        kwrite_notify("kdeglobals", &["Icons"], "Theme", theme)?;
    }
    refresh_icons(false);
    Ok(())
}

/// Lumen's File Explorer for this session without logging out. The packaged Dolphin comes first through the session
/// env script, which only runs at login: the session's service and D-Bus environment get the same PATH and data dirs
/// now. Either way the file manager service restarts on the Lumen binary. Returns whether the environment changed.
fn activate_file_manager() -> bool {
    try_run("systemctl", &["--user", "daemon-reload"]);
    let mut changed = false;
    if let Some(bin) = Path::new(PACKAGED_DOLPHIN).parent().filter(|_| Path::new(PACKAGED_DOLPHIN).is_file()) {
        // The session's own environment, not this process's (a terminal's PATH has shell additions).
        let session = output("systemctl", &["--user", "show-environment"]).unwrap_or_default();
        let get = |var: &str, default: &str| {
            session.lines().find_map(|l| l.strip_prefix(&format!("{var}="))).filter(|v| !v.is_empty()).unwrap_or(default).to_owned()
        };
        let mut vars = Vec::new();
        for (var, dir, default) in [("PATH", bin.to_string_lossy().into_owned(), "/usr/local/sbin:/usr/local/bin:/usr/bin"), ("XDG_DATA_DIRS", PACKAGED_XDG.to_owned(), "/usr/local/share:/usr/share")] {
            let current = get(var, default);
            if !current.split(':').any(|d| d == dir) {
                vars.push(format!("{var}={dir}:{current}"));
            }
        }
        if !vars.is_empty() {
            let mut args = vec!["--user", "set-environment"];
            args.extend(vars.iter().map(String::as_str));
            try_run("systemctl", &args);
            try_run("dbus-update-activation-environment", &vars.iter().map(String::as_str).collect::<Vec<_>>());
            let data_dirs = vars.iter().find_map(|v| v.strip_prefix("XDG_DATA_DIRS=")).map(str::to_owned).unwrap_or_else(|| get("XDG_DATA_DIRS", ""));
            let _ = Command::new("kbuildsycoca6").env("XDG_DATA_DIRS", data_dirs).stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null()).status();
            changed = true;
        }
    }
    // Restarting closes the service's windows: only when the running file manager is not Lumen's (an upgraded binary
    // shows as "… (deleted)" and keeps running).
    let pid = output("systemctl", &["--user", "show", "--property=MainPID", "--value", "plasma-dolphin.service"]).unwrap_or_default();
    let exe = std::fs::read_link(format!("/proc/{pid}/exe")).map(|p| p.to_string_lossy().trim_end_matches(" (deleted)").to_owned());
    if pid != "0" && exe.is_ok_and(|e| e != PACKAGED_DOLPHIN && e != DOLPHIN_BIN) {
        try_run("systemctl", &["--user", "try-restart", "plasma-dolphin.service"]);
    }
    changed
}

/// Widgets in a Lumen panel that Lumen supersedes: (old plugin, replacement or "" to drop, carry settings over).
pub const PANEL_SWAPS: &[(&str, &str, bool)] = &[
    ("org.kde.plasma.icontasks", "org.lumen.tasks", true),
    ("org.kde.plasma.taskmanager", "org.lumen.tasks", true),
    ("org.kde.plasma.panelspacer", "org.lumen.spacer", true),
    ("luisbocanegra.panelspacer.extended", "org.lumen.spacer", false),
    ("luisbocanegra.panel.colorizer", "", false),
];

/// Lumen trays host org.lumen.notifier (notification server, Windows toasts) instead of Plasma's Notifications.
/// The stock id stays known, or the tray would enable it again as a new plugin.
fn adopt_lumen_notifier() {
    const SCRIPT: &str = "panels().forEach(function (p) { p.widgets('org.lumen.tray').forEach(function (t) {\
        t.currentConfigGroup = ['General'];\
        var list = function (k) { var v = t.readConfig(k); return Array.isArray(v) ? v.slice() : (v ? String(v).split(',') : []); };\
        var changed = false;\
        ['extraItems', 'itemOrder', 'shownItems', 'hiddenItems', 'suppressedItems'].forEach(function (k) {\
            var l = list(k), i = l.indexOf('org.kde.plasma.notifications');\
            if (i < 0 || l.indexOf('org.lumen.notifier') >= 0 && k != 'extraItems') return;\
            if (l.indexOf('org.lumen.notifier') >= 0) l.splice(i, 1); else if (k == 'suppressedItems') l.push('org.lumen.notifier'); else l[i] = 'org.lumen.notifier';\
            t.writeConfig(k, l); changed = true;\
        });\
        var known = list('knownItems');\
        ['org.kde.plasma.notifications', 'org.lumen.notifier'].forEach(function (id) { if (known.indexOf(id) < 0) { known.push(id); changed = true; } });\
        if (changed) { t.writeConfig('knownItems', known); t.reloadConfig(); print('tray: org.kde.plasma.notifications -> org.lumen.notifier\\n'); }\
    }); });";
    if let Some(out) = output("qdbus6", &["org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", SCRIPT]) {
        for line in out.lines().filter(|l| l.contains(" -> ")) {
            println!("{line}");
        }
    }
}

/// Lumen widgets back to Plasma's own on uninstall, in the same spot.
const PANEL_RESTORE: &[(&str, &str, bool)] = &[
    ("org.lumen.tasks", "org.kde.plasma.icontasks", true),
    ("org.lumen.spacer", "org.kde.plasma.panelspacer", true),
    ("org.lumen.start", "org.kde.plasma.kickoff", false),
    ("org.lumen.clock", "org.kde.plasma.digitalclock", false),
    ("org.lumen.tray", "org.kde.plasma.systemtray", false),
];

/// Swaps superseded widgets in Lumen panels for Lumen's own in the same spot (task manager settings and
/// pinned apps carried over). Runs before their packages go, while Plasma can still load them.
fn adopt_lumen_widgets() {
    swap_widgets(PANEL_SWAPS, true);
}

fn swap_widgets(swaps: &[(&str, &str, bool)], lumen_panels: bool) {
    let table: Vec<String> = swaps.iter().map(|(old, new, carry)| format!("['{old}','{new}',{carry}]")).collect();
    let script = format!(
        "var swaps = [{}];\
        panels().forEach(function (p) {{\
            if ({lumen_panels} && !p.widgets('org.lumen.start').length) return;\
            swaps.forEach(function (s) {{\
                p.widgets(s[0]).forEach(function (old) {{\
                    var idx = old.index, vals = {{}}, w = null;\
                    if (s[2]) {{ old.currentConfigGroup = ['General']; old.configKeys.forEach(function (k) {{ vals[k] = old.readConfig(k); }}); }}\
                    if (s[1]) {{ w = p.addWidget(s[1]); w.currentConfigGroup = ['General']; for (var k in vals) w.writeConfig(k, vals[k]); w.reloadConfig(); }}\
                    old.remove();\
                    if (w) w.index = idx;\
                    print(s[0] + ' -> ' + (s[1] || 'removed') + '\\n');\
                }});\
            }});\
        }});",
        table.join(",")
    );
    if let Some(out) = output("qdbus6", &["org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", &script]) {
        for line in out.lines().filter(|l| l.contains(" -> ")) {
            println!("panel: {line}");
        }
    }
}

/// Settings the layout script carried over from the old panel count as the user's taskbar choices.
fn keep_layout_choices(snap: &crate::layout::Snapshot) -> Result<()> {
    let get = |k: &str| snap.layout.get(k).map(String::as_str);
    if get("clockDateFormat").is_some_and(|f| !f.is_empty()) {
        kwrite("lumenrc", &["Taskbar"], "clockDateStyle", "4")?;
    }
    if get("clockShowSeconds") == Some("true") {
        kwrite("lumenrc", &["Taskbar"], "clockSeconds", "true")?;
    }
    if get("hiding") == Some("autohide") {
        kwrite("lumenrc", &["Taskbar"], "autoHide", "true")?;
    }
    Ok(())
}

/// Taskbar settings: the user's choices (lumenrc [Taskbar], written by the settings module), defaults for the rest.
fn apply_taskbar(t: &crate::tokens::Tokens) {
    use crate::generate::kcfg::{Bar, taskbar, taskbar_js};
    let rc = crate::layout::Ini::parse(&std::fs::read_to_string(config_home().join("lumenrc")).unwrap_or_default());
    let values: Vec<String> = taskbar(t)
        .iter()
        .map(|(key, _, _, d)| {
            let chosen = rc.get(&["Taskbar"], key);
            let v = match d {
                Bar::Int(n) => chosen.and_then(|v| v.trim().parse::<i64>().ok()).unwrap_or(*n).to_string(),
                Bar::Bool(b) => chosen.map_or(*b, |v| v.trim() == "true").to_string(),
            };
            format!("\"{key}\":{v}")
        })
        .collect();
    let script = format!("{}var p = lumenPanel(); if (p) writeBar(p, {{{}}});", taskbar_js(t), values.join(","));
    try_run("qdbus6", &["org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", &script]);
}

/// Plasma's system default keeps notifications without a desktop file out of history; Win11's center lists them.
/// Applied only while the user hasn't chosen, so the Notifications settings page still decides.
fn keep_other_notifications() {
    let text = std::fs::read_to_string(config_home().join("plasmanotifyrc")).unwrap_or_default();
    let mut other = false;
    for line in text.lines().map(str::trim) {
        if line.starts_with('[') {
            other = line == "[Applications][@other]";
        } else if other && line.starts_with("ShowInHistory") {
            return;
        }
    }
    let _ = kwrite("plasmanotifyrc", &["Applications", "@other"], "ShowInHistory", "true");
}

/// An app pinned to a scheme in its own color menu ignores the system scheme, and with it light/dark switches.
fn unpin_app_schemes() {
    let Ok(dir) = std::fs::read_dir(config_home()) else { return };
    for path in dir.flatten().map(|e| e.path()).filter(|p| p.is_file() && p.file_name().is_some_and(|n| n.to_string_lossy().ends_with("rc"))) {
        if !std::fs::read_to_string(&path).is_ok_and(|t| t.contains("[UiSettings]")) {
            continue;
        }
        let file = path.to_string_lossy();
        if let Some(v) = output("kreadconfig6", &["--file", &file, "--group", "UiSettings", "--key", "ColorScheme"])
            && (v.starts_with("Klassy") || v.starts_with("Kv") || v.starts_with("Lumen"))
            && try_run("kwriteconfig6", &["--file", &file, "--group", "UiSettings", "--key", "ColorScheme", "--delete"])
        {
            println!("{}: unpinned color scheme {v}", path.file_name().unwrap().to_string_lossy());
        }
    }
}

fn gtk(ctx: &Ctx, variant: &str) -> Result<()> {
    let theme = format!("Lumen-{}", if variant == "dark" { "Dark" } else { "Light" });
    if !try_run("qdbus6", &["org.kde.GtkConfig", "/GtkConfig", "org.kde.GtkConfig.setGtkTheme", &theme]) {
        for v in ["gtk-3.0", "gtk-4.0"] {
            kwrite(&config_home().join(v).join("settings.ini").to_string_lossy(), &["Settings"], "gtk-theme-name", &theme)?;
        }
    }
    let gtk4 = config_home().join("gtk-4.0");
    write_file(&gtk4.join("lumen.css"), &std::fs::read(ctx.out_path(format!("config/gtk-4.0/lumen-{variant}.css")))?)?;
    let user_css = gtk4.join("gtk.css");
    let cur = std::fs::read_to_string(&user_css).unwrap_or_default();
    if !cur.contains("lumen.css") {
        write_file(&user_css, format!("{cur}{}@import 'lumen.css';\n", if cur.is_empty() || cur.ends_with('\n') { "" } else { "\n" }).as_bytes())?;
    }
    try_run("gsettings", &["set", "org.gnome.desktop.interface", "color-scheme", if variant == "dark" { "prefer-dark" } else { "prefer-light" }]);
    Ok(())
}

pub fn doctor(ctx: &Ctx) -> Result<()> {
    let mut ok = true;
    let mut check = |name: &str, good: bool, hint: &str| {
        println!("{} {name}{}", if good { "ok  " } else { "FAIL" }, if good { String::new() } else { format!(" — {hint}") });
        ok &= good;
    };
    let fix = if ctx.packaged { "reinstall the package (`yay -S lumen-kde-git`)" } else { "run `lumen install --system`" };
    let qt = output("qtpaths6", &["--query", "QT_INSTALL_PLUGINS"]).unwrap_or_else(|| "/usr/lib/qt6/plugins".into());
    check("Qt style plugin", Path::new(&qt).join("styles/lumen6.so").is_file(), fix);
    check("decoration plugin", Path::new(&qt).join("org.kde.kdecoration3/org.lumen.decoration.so").is_file(), fix);
    check("tray plugin", Path::new(&qt).join("plasma/applets/org.lumen.tray.so").is_file(), fix);
    check("settings module", Path::new(&qt).join("plasma/kcms/systemsettings/kcm_lumen.so").is_file(), fix);
    check("Qt Quick settings bridge", Path::new(&qml_dir()).join("org/lumen/settings/liblumenqmlsettings.so").is_file(), fix);
    check("lock/logout overlay", Path::new(SHELL_DST).join("contents/lockscreen/MainBlock.qml").is_file(), "run `sudo lumen refresh-shell`");
    let dolphin = |bin: &str, dropin: &str| Path::new(bin).is_file() && Path::new(dropin).is_file();
    check("Lumen Dolphin", dolphin(DOLPHIN_BIN, DOLPHIN_DROPIN) || dolphin(PACKAGED_DOLPHIN, PACKAGED_DOLPHIN_DROPIN), fix);
    check(
        "client title row rule",
        output("kreadconfig6", &["--file", "kwinrulesrc", "--group", TITLEBAR_RULE, "--key", "decorationpolicy"]).as_deref() == Some("shadow"),
        "run `lumen apply`",
    );
    for p in ["org.lumen.start", "org.lumen.clock", "org.lumen.spacer"] {
        let rel = Path::new("plasma/plasmoids").join(p);
        check(p, data_home().join(&rel).is_dir() || Path::new("/usr/share").join(&rel).is_dir(), "run `lumen install`");
    }
    for (p, what) in [
        ("plasma/desktoptheme/lumen-dark", "plasma style"),
        ("plasma/look-and-feel/org.lumen.desktop.dark", "global theme"),
        ("color-schemes/LumenDark.colors", "color scheme"),
    ] {
        check(what, data_home().join(p).exists() || Path::new("/usr/share").join(p).exists(), "run `lumen install`");
    }
    let stale = crate::layout::superseded_in_lumen_panels(&crate::layout::appletsrc(), PANEL_SWAPS.iter().map(|s| s.0));
    check("panel widgets", stale.is_empty(), &format!("{} superseded; `lumen apply` swaps them for Lumen's own", stale.join(", ")));
    let loaded = output("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.isEffectLoaded", "lumen_materials"]).is_some_and(|s| s == "true");
    let wanted = output("kreadconfig6", &["--file", "kwinrc", "--group", "Plugins", "--key", "lumen_materialsEnabled"]).is_some_and(|s| s == "true");
    let rebuild = if ctx.packaged { "falling back to stock blur; rebuild the package (`yay -S lumen-kde-git`)" } else { "falling back to stock blur; rebuild with `sudo lumen rebuild-kwin`" };
    check(if wanted { "KWin materials effect loaded" } else { "KWin materials effect (not enabled yet)" }, loaded || !wanted, rebuild);
    if wanted && !loaded {
        kwrite("kwinrc", &["Plugins"], "blurEnabled", "true")?;
        try_quiet("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.loadEffect", "blur"]);
    }
    if !ok {
        bail!("some checks failed");
    }
    Ok(())
}

/// Packages whose job Lumen takes over. Removal needs explicit confirmation.
const SUPERSEDED: &[&str] = &[
    "klassy",
    "kvantum",
    "kwin-effects-glass-git",
    "kwin-effects-forceblur",
    "kwin-effects-better-blur-dx",
    "kwin-effects-better-blur-dx-git",
    "kde-rounded-corners",
    "kwin-scripts-shapecorners",
    "plasma6-applets-panel-colorizer",
    "plasma6-applets-panel-spacer-extended",
    "adapta-gtk-theme",
    "cachyos-emerald-kde-theme-git",
    "cachyos-nord-kde-theme-git",
];
/// Per-user settings the superseded packages leave behind.
const LEFTOVER_CONFIG: &[(&str, &str)] = &[("klassy", "klassy"), ("kvantum", "Kvantum"), ("plasma6-applets-panel-colorizer", "panel-colorizer")];

pub fn cleanup(_ctx: &Ctx, yes: bool) -> Result<()> {
    let installed: Vec<&str> = SUPERSEDED.iter().copied().filter(|p| try_quiet("pacman", &["-Qq", p])).collect();
    if installed.is_empty() {
        println!("nothing to clean up");
        return Ok(());
    }
    println!("Lumen replaces these installed packages:\n  {}", installed.join("\n  "));
    if !yes {
        print!("Remove them with `pacman -Rns`? [y/N] ");
        std::io::Write::flush(&mut std::io::stdout())?;
        let mut answer = String::new();
        std::io::stdin().read_line(&mut answer)?;
        if !matches!(answer.trim(), "y" | "Y" | "yes") {
            println!("kept");
            return Ok(());
        }
    }
    adopt_lumen_widgets();
    let mut args = vec!["-Rns"];
    if yes {
        args.push("--noconfirm");
    }
    args.extend(installed.iter().copied());
    root_cmd("pacman", &args)?;
    for (pkg, dir) in LEFTOVER_CONFIG {
        if installed.contains(pkg) {
            let _ = std::fs::remove_dir_all(config_home().join(dir));
        }
    }
    Ok(())
}

fn try_quiet(cmd: &str, args: &[&str]) -> bool {
    Command::new(cmd).args(args).stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null()).status().is_ok_and(|s| s.success())
}

pub fn rebuild_kwin(ctx: &Ctx) -> Result<()> {
    if ctx.packaged {
        // As a pacman hook (NeedsTargets) stdin lists the updated packages: a Lumen build in the same transaction is current.
        let mut targets = String::new();
        if !std::io::IsTerminal::is_terminal(&std::io::stdin()) {
            let _ = std::io::Read::read_to_string(&mut std::io::stdin(), &mut targets);
        }
        if !targets.lines().any(|t| PACKAGES.contains(&t.trim())) {
            println!("Lumen's KWin, Qt and Plasma plugins were built for the previous versions: rebuild the package (`yay -S lumen-kde-git`).");
            println!("Until then `lumen doctor` switches the effects back to stock blur.");
        }
        return Ok(());
    }
    let dir = PathBuf::from("/var/cache/lumen/build");
    std::fs::create_dir_all(&dir)?;
    // pacman keeps the packaged mtimes, so AUTOMOC may judge moc files current and keep stale plugin IIDs
    // (KWin refuses an effect whose IID names another KWin version).
    let autogen: Vec<PathBuf> = walkdir::WalkDir::new(&dir)
        .max_depth(6)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_dir() && e.file_name().to_string_lossy().ends_with("_autogen"))
        .map(|e| e.into_path())
        .collect();
    for d in autogen {
        let _ = std::fs::remove_dir_all(d);
    }
    build_native_in(ctx, &dir)?;
    run("cmake", &["--install", &dir.to_string_lossy()])
}

/// Qt and KDE packages the installed binaries link against: an update of any of them rebuilds Lumen.
fn linked_packages(build: &Path) -> Vec<String> {
    let mut pkgs: Vec<String> = ["kwin", "kdecoration", "qt6-base", "libplasma"].map(String::from).to_vec();
    let Ok(manifest) = std::fs::read_to_string(build.join("install_manifest.txt")) else { return pkgs };
    let elf = |f: &&str| std::fs::File::open(f).and_then(|mut h| {
        let mut magic = [0u8; 4];
        std::io::Read::read_exact(&mut h, &mut magic).map(|_| magic == *b"\x7fELF")
    });
    let bins: Vec<&str> = manifest.lines().filter(|f| elf(f).unwrap_or(false)).collect();
    if bins.is_empty() {
        return pkgs;
    }
    let mut libs: Vec<String> = output("ldd", &bins)
        .unwrap_or_default()
        .lines()
        .filter_map(|l| l.split_once("=> ").and_then(|(_, r)| r.split_whitespace().next()).filter(|p| p.starts_with('/')).map(String::from))
        .collect();
    libs.sort_unstable();
    libs.dedup();
    let mut args = vec!["-Qqo"];
    args.extend(libs.iter().map(String::as_str));
    let owners = Command::new("pacman").args(&args).output().map(|o| String::from_utf8_lossy(&o.stdout).into_owned()).unwrap_or_default();
    let mut owners: Vec<&str> = owners.lines().collect();
    owners.sort_unstable();
    owners.dedup();
    let mut args = vec!["-Qi"];
    args.extend(owners.iter().copied());
    let info = Command::new("pacman").args(&args).env("LC_ALL", "C").output().map(|o| String::from_utf8_lossy(&o.stdout).into_owned()).unwrap_or_default();
    let mut name = "";
    for (k, v) in info.lines().filter_map(|l| l.split_once(':')) {
        match k.trim() {
            "Name" => name = v.trim(),
            "URL" if name.starts_with("qt6-") || v.contains("kde.org") => pkgs.push(name.to_owned()),
            _ => {}
        }
    }
    pkgs.sort_unstable();
    pkgs.dedup();
    pkgs
}

/// Puts the session back on Breeze while Lumen's plugins are still installed: panel widgets swapped in place, global
/// theme, decoration, effects, fonts, GTK and the lock screen wallpaper. Icons and cursors stay the user's.
fn restore_stock(ctx: &Ctx) -> Result<()> {
    let session = std::env::var_os("WAYLAND_DISPLAY").is_some() || std::env::var_os("DISPLAY").is_some();
    if !session || !try_quiet("qdbus6", &["org.kde.plasmashell", "/PlasmaShell"]) {
        println!("no running Plasma session: theme left as is; pick a Global Theme in System Settings after logging in");
        return Ok(());
    }
    swap_widgets(PANEL_RESTORE, false);
    let light = output("kreadconfig6", &["--file", "kdeglobals", "--group", "KDE", "--key", "LookAndFeelPackage"]).as_deref() == Some("org.lumen.desktop.light");
    let lnf = if light { "org.kde.breeze.desktop" } else { "org.kde.breezedark.desktop" };
    keep_user_themes(|| run("plasma-apply-lookandfeel", &["--apply", lnf]))?;
    for key in ["DefaultDarkLookAndFeel", "DefaultLightLookAndFeel", "ShowIconsOnPushButtons"] {
        reset_key("kdeglobals", &["KDE"], key)?;
    }
    let t = &ctx.t;
    let mut families: Vec<String> = ["font.ui", "font.ui_small", "font.win32", "font.win32_mono", "font.mono"].iter().map(|k| t.s(k).to_owned()).collect();
    families.extend(t.sv("font.fallback"));
    for (group, key) in [("General", "font"), ("General", "menuFont"), ("General", "toolBarFont"), ("General", "smallestReadableFont"), ("General", "fixed"), ("WM", "activeFont")] {
        let current = output("kreadconfig6", &["--file", "kdeglobals", "--group", group, "--key", key]).unwrap_or_default();
        if current.split(',').next().is_some_and(|f| families.iter().any(|x| x == f)) {
            reset_key("kdeglobals", &[group], key)?;
        }
    }
    try_run("dbus-send", &["--session", "--type=signal", "/KDEPlatformTheme", "org.kde.KDEPlatformTheme.refreshFonts"]);
    let deco = ["org.kde.kdecoration2"];
    if output("kreadconfig6", &["--file", "kwinrc", "--group", deco[0], "--key", "library"]).as_deref() == Some(DECORATION_LIBRARY) {
        for key in ["library", "theme", "ButtonsOnLeft", "ButtonsOnRight"] {
            reset_key("kwinrc", &deco, key)?;
        }
    }
    for key in ["lumen_materialsEnabled", "lumen_openEnabled", "lumen_minimizeEnabled"] {
        kdelete("kwinrc", &["Plugins"], key)?;
    }
    // Stock effects `apply` switched off; third-party ones stay off (their packages may be gone).
    for key in ["blurEnabled", "contrastEnabled", "scaleEnabled", "glideEnabled", "squashEnabled", "magiclampEnabled"] {
        reset_key("kwinrc", &["Plugins"], key)?;
    }
    let rules = output("kreadconfig6", &["--file", "kwinrulesrc", "--group", "General", "--key", "rules"]).unwrap_or_default();
    let ids: Vec<&str> = rules.split(',').filter(|s| !s.is_empty() && *s != TITLEBAR_RULE).collect();
    if rules.split(',').any(|s| s == TITLEBAR_RULE) {
        kwrite("kwinrulesrc", &["General"], "rules", &ids.join(","))?;
        kwrite("kwinrulesrc", &["General"], "count", &ids.len().to_string())?;
    }
    lock_wallpaper(false)?;
    let saved = output("kreadconfig6", &["--file", "lumenrc", "--group", "Restore", "--key", "IconTheme"]).filter(|t| !t.is_empty());
    let current = output("kreadconfig6", &["--file", "kdeglobals", "--group", "Icons", "--key", "Theme"]).unwrap_or_default();
    if let Some(previous) = saved
        && current == ICON_THEME
    {
        set_icon_theme(&previous)?;
        println!("icon theme: back to {previous}");
    }
    for key in ["IconTheme", "FirstApplied", "AppliedVersion"] {
        kdelete("lumenrc", &["Restore"], key)?;
    }
    if !try_run("qdbus6", &["org.kde.GtkConfig", "/GtkConfig", "org.kde.GtkConfig.setGtkTheme", "Breeze"]) {
        for v in ["gtk-3.0", "gtk-4.0"] {
            let ini = config_home().join(v).join("settings.ini");
            if ini.is_file() {
                kwrite(&ini.to_string_lossy(), &["Settings"], "gtk-theme-name", "Breeze")?;
            }
        }
    }
    try_run("qdbus6", &["org.kde.KWin", "/KWin", "reconfigure"]);
    for e in ["lumen_materials", "lumen_open", "lumen_minimize"] {
        try_run("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.unloadEffect", e]);
    }
    try_run("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.loadEffect", "blur"]);
    println!("switched back to Breeze ({lnf})");
    Ok(())
}

/// Lumen never picks icon or cursor themes: keeps the ones in effect across a look-and-feel switch, which can
/// drop values that only live in kdedefaults.
fn keep_user_themes(switch: impl FnOnce() -> Result<()>) -> Result<()> {
    const USER_THEMES: [(&str, &str, &str); 3] = [("kdeglobals", "Icons", "Theme"), ("kcminputrc", "Mouse", "cursorTheme"), ("kcminputrc", "Mouse", "cursorSize")];
    let kept: Vec<Option<String>> = USER_THEMES.iter().map(|(f, g, k)| output("kreadconfig6", &["--file", f, "--group", g, "--key", k])).collect();
    switch()?;
    for ((file, group, key), value) in USER_THEMES.iter().zip(&kept) {
        if let Some(v) = value.as_deref().filter(|v| !v.is_empty())
            && output("kreadconfig6", &["--file", file, "--group", group, "--key", key]).as_deref() != Some(v)
        {
            kwrite(file, &[group], key, v)?;
        }
    }
    Ok(())
}

pub fn uninstall(ctx: &Ctx, system: bool) -> Result<()> {
    restore_stock(ctx)?;
    remove_manifest(&manifest_path())?;
    qqc2_env(false)?;
    restore_icon_colors();
    let _ = std::fs::remove_dir_all(data_home().join("lumen"));
    let _ = std::fs::remove_dir_all(data_home().join("fonts/lumen-windows"));
    let _ = std::fs::remove_file(user_font_conf());
    let gtk4 = config_home().join("gtk-4.0");
    let _ = std::fs::remove_file(gtk4.join("lumen.css"));
    if let Ok(css) = std::fs::read_to_string(gtk4.join("gtk.css")) {
        let kept: String = css.lines().filter(|l| !l.contains("lumen.css")).map(|l| format!("{l}\n")).collect();
        write_file(&gtk4.join("gtk.css"), kept.as_bytes())?;
    }
    if system && ctx.packaged {
        println!("system files belong to the package: remove it with `sudo pacman -Rns {}`", PACKAGES.iter().find(|p| try_quiet("pacman", &["-Qq", p])).unwrap_or(&PACKAGES[1]));
    } else if system {
        boot(ctx, false)?;
        login(ctx, false)?;
        let esp = esp_path();
        root_cmd(
            "rm",
            &[
                "-rf",
                KWIN_HOOK,
                SHELL_HOOK,
                SHELL_DST,
                SYS_STATE,
                "/etc/systemd/system/lumen-vtrgb.service",
                "/usr/share/plymouth/themes/lumen",
                "/usr/share/plasma/desktoptheme/lumen-dark",
                "/usr/share/plasma/desktoptheme/lumen-light",
                "/usr/share/plasma/look-and-feel/org.lumen.desktop.dark",
                "/usr/share/plasma/look-and-feel/org.lumen.desktop.light",
                "/usr/share/color-schemes/LumenDark.colors",
                "/usr/share/color-schemes/LumenLight.colors",
                "/usr/share/wallpapers/Lumen",
                "/usr/share/sounds/lumen",
                "/usr/share/themes/Lumen-Dark",
                "/usr/share/themes/Lumen-Light",
                "/usr/local/bin/lumen",
                DOLPHIN_DROPIN,
                &qqc2_paths().0,
                &qqc2_paths().1,
                &kirigami_style_path(),
                KIRIGAMI_SHADOW,
                WIN_FONTS,
                &format!("/etc/fonts/conf.d/{FONT_CONF}"),
                &format!("{esp}/lumen.png"),
            ],
        )?;
        let manifest = PathBuf::from("/var/cache/lumen/build/install_manifest.txt");
        let local = ctx.root.join("build/cmake/install_manifest.txt");
        if let Some(m) = [manifest, local].into_iter().find(|p| p.is_file()) {
            for f in std::fs::read_to_string(m)?.lines() {
                root_cmd("rm", &["-f", f])?;
            }
        }
    }
    println!("uninstalled; switch Global Theme back in System Settings if Lumen was active");
    Ok(())
}
