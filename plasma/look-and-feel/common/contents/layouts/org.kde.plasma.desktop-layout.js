// Lumen taskbar. `lumen apply --layout` snapshots the previous panel into ~/.config/lumen-layoutrc
// ([Layout] + [LayoutWidget-N]) right before this runs, so existing extras, tray choices,
// pinned launchers and Start favorites carry over.
var snap = ConfigFile("lumen-layoutrc", "Layout");

function entry(key, fallback) {
    var v = snap.readEntry(key);
    return v === undefined || v === null || v === "" ? fallback : String(v);
}

function writeAll(widget, group, pairs) {
    widget.currentConfigGroup = group;
    for (var k in pairs) {
        if (pairs[k] !== undefined && pairs[k] !== "") {
            widget.writeConfig(k, pairs[k]);
        }
    }
    widget.reloadConfig();
}

// Re-create an extra widget with the configuration captured from the old panel.
function addExtra(panel, index) {
    var cfg = ConfigFile("lumen-layoutrc", "LayoutWidget-" + index);
    var plugin = cfg.readEntry("plugin");
    if (!plugin || knownWidgetTypes.indexOf(String(plugin)) < 0) {
        return;
    }
    var w = panel.addWidget(String(plugin));
    var keys = String(cfg.readEntry("keys") || "").split("\u001f");
    for (var i = 0; i < keys.length; ++i) {
        if (keys[i] === "") {
            continue;
        }
        var parts = keys[i].split("/");
        var key = parts.pop();
        w.currentConfigGroup = parts;
        w.writeConfig(key, cfg.readEntry(keys[i]));
    }
    var shortcut = cfg.readEntry("shortcut");
    if (shortcut) {
        w.globalShortcut = String(shortcut);
    }
    w.reloadConfig();
}

function addExtras(panel, key) {
    var ids = entry(key, "").split(",");
    for (var i = 0; i < ids.length; ++i) {
        if (ids[i] !== "") {
            addExtra(panel, ids[i]);
        }
    }
}

var panel = new Panel;
panel.location = "bottom";
panel.height = @@size.taskbar_h@@;
panel.lengthMode = "fill";
panel.floating = false;
panel.hiding = entry("hiding", "none");
panel.opacity = "translucent";

addExtras(panel, "leftWidgets");
panel.addWidget("org.lumen.spacer");

var start = panel.addWidget("org.lumen.start");
writeAll(start, ["General"], {
    favoritesClientId: entry("startFavoritesClient", ""),
    favoritesPortedToKAstats: entry("startFavoritesClient", "") !== "" ? "true" : "",
});
start.globalShortcut = entry("startShortcut", "Alt+F1");

var tasks = panel.addWidget("org.lumen.tasks");
writeAll(tasks, ["General"], {
    launchers: entry("launchers", "applications:systemsettings.desktop,preferred://filemanager,preferred://browser"),
    iconSpacing: "0",
    maxStripes: "1",
    groupedTaskVisualization: "1",
});

addExtras(panel, "centerWidgets");
panel.addWidget("org.lumen.spacer");
addExtras(panel, "rightWidgets");

var tray = panel.addWidget("org.lumen.tray");
writeAll(tray, ["General"], {
    extraItems: entry("trayExtraItems", ""),
    knownItems: entry("trayKnownItems", ""),
    shownItems: entry("trayShownItems", ""),
    hiddenItems: entry("trayHiddenItems", ""),
    disabledStatusNotifiers: entry("trayDisabledSNIs", ""),
});

var clock = panel.addWidget("org.lumen.clock");
writeAll(clock, ["General"], {
    dateFormat: entry("clockDateFormat", ""),
    dateStyle: entry("clockDateFormat", "") !== "" ? "4" : "",
    showSeconds: entry("clockShowSeconds", ""),
});
