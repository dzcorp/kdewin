# Plasma 6.8.2 plasmoid QML cheat sheet (this machine: plasma-* 6.8.2-1.1, KF 6.31, Qt 6.12)

## Method and conventions
- Type and property names come from installed `*.qmltypes` under `/usr/lib/qt6/qml/org/kde/...`. Almost all stock applet QML is compiled into `.so` files, so snippets cite the upstream **v6.8.2 tag tarballs** from `https://invent.kde.org/plasma/<repo>/-/archive/v6.8.2/<repo>-v6.8.2.tar.gz`.
- Extracted to a scratch `up/<repo>/` directory. Repo abbreviations used below:
  - PW=plasma-workspace, PD=plasma-desktop, NM=plasma-nm, PA=plasma-pa, BD=bluedevil
  - PDV=powerdevil, LP=libplasma, KW=kwin, KSL=kscreenlocker, P5S=plasma5support
- A small qmltypes dumper (`qt.py`, not in the repo) was used: `python3 -I qt.py X.qmltypes [TypeName...]`.
- All imports are versionless (Qt 6 style). "Installed" means a path under `/usr/share` or `/usr/lib/qt6`.
- Evidence that third-party packages can import the "private" modules on this machine: `~/.local/share/plasma/plasmoids/KdeControlStation` imports volume, brightnesscontrolplugin, mpris, sessions, notificationmanager, networkmanagement and bluezqt. `AndromedaLauncher` and `org.kde.plasma.simplekickoff` import `org.kde.plasma.private.kicker`. Nothing in the QML engine blocks them. These are unstable APIs.

### Findings that change how you build things in 6.8.2
1. **Lockscreen, logout and OSD are not look-and-feel (L&F) items any more.**
   - The L&F structure only defines `defaults`, `layouts`, `plasmoidsetupscripts`, `colors`, `previews`, `logout`, `splash` and `windowswitcher` (PW `shell/packageplugins/lookandfeel/lookandfeel.cpp:25-47`).
   - Lockscreen: `kscreenlocker_greet` loads `lockscreenmainscript` from the **Plasma/Shell** package (KSL `greeter/greeterapp.cpp:505-516`; LP `shellpackage.cpp:64`).
   - Logout: `logout-greeter` loads `logoutmainscript` from the shell package named by `plasmashellrc [Shell] ShellPackage`, default `org.kde.plasma.desktop` (PW `logout-greeter/main.cpp:28-40,85`; `shutdowndlg.cpp:217`).
   - OSD: `shell/osd.cpp:239` hard-loads the QML module `org.kde.plasma.workspace.osd` type `Osd`. `osdmainscript` is defined in the shell package but nothing reads it.
   - Consequence (inferred): the `contents/{lockscreen,logout,osd}` dirs in installed L&F packages `CachyOS-Nord` and `look-and-feel/Iridescent-round` are ignored by 6.8.2. To customize them you need a custom Plasma/Shell package. `X-Plasma-FallbackPackage` selects its fallback (LP `shellpackage.cpp:62-67`).
2. **L&F windowswitcher does work.** KWin first checks `plasma/look-and-feel/<LayoutName>/contents/windowswitcher/WindowSwitcher.qml`, then `kwin/tabbox/<LayoutName>` (KW `src/tabbox/tabboxhandler.cpp:166-182`).
3. **No `statusnotifieritem` Plasma5Support engine exists.** The installed engines are activities, apps, bbcukmet, devicenotifications, dwd, envcan, executable, favicons, filebrowser, geolocation, hotplug, keystate, mouse, noaa, packagekit, places, powermanagement, soliddevice, time, weather and wettercom (listing of `/usr/lib/qt6/plugins/plasma5support/dataengine`).
4. **System tray click, context-menu and scroll methods are not importable.** They live on the C++ `SystemTray` applet (see §5).

---
## 1. Plasmoid basics

**Imports:** `org.kde.plasma.plasmoid` (PlasmoidItem, `Plasmoid` attached), `org.kde.plasma.core as PlasmaCore` (Types, Action, Dialog, PopupPlasmaWindow, ToolTipArea), `org.kde.plasma.components as PC3`, `org.kde.kirigami as Kirigami`.

**Layout and metadata**
- Layout: `~/.local/share/plasma/plasmoids/<Id>/{metadata.json, contents/ui/main.qml, contents/config/{main.xml,config.qml}}`.
- `ui/main.qml` is the default mainscript (LP `packagestructure/packages.cpp:23`); the config files come from `plasma_applet_packagestructure.cpp:23-25`.
- Config page `source:` paths are relative to `contents/ui/`. Installed example: `~/.local/share/plasma/plasmoids/KdeControlStation`.
- Minimal `metadata.json`. Keys are copied from `/usr/share/plasma/plasmoids/org.kde.plasma.icontasks/metadata.json` and `.../luisbocanegra.panelspacer.extended/metadata.json`:
```json
{ "KPackageStructure": "Plasma/Applet",
  "KPlugin": { "Id": "org.example.hello", "Name": "Hello", "Description": "x", "Icon": "start-here-kde",
               "Category": "Utilities", "License": "GPL-2.0+", "Version": "1.0",
               "Authors": [{"Name": "me"}], "EnabledByDefault": true },
  "X-Plasma-API-Minimum-Version": "6.0",
  "X-Plasma-Provides": ["org.kde.plasma.launchermenu"] }
```
- `X-Plasma-Provides` is optional; it feeds the "alternatives" dialog. Kickoff uses `launchermenu` and icontasks uses `multitasking`.
- `X-Plasma-RootPath` lets a package reuse another's contents (icontasks points at `org.kde.plasma.taskmanager`).
- To be listable inside the system tray, the key is `X-Plasma-NotificationAreaCategory` (e.g. `SystemServices` or `Hardware`; PW `systemtray/plasmoidregistry.cpp:114`). The `X-Plasma-NotificationArea` key that KdeControlStation uses is not read by 6.8.2.

**`contents/config/main.xml`** (StringList copied from PD `applets/kickoff/main.xml:20`):
```xml
<kcfg xmlns="http://www.kde.org/standards/kcfg/1.0"><kcfgfile name=""/>
 <group name="General">
  <entry name="names" type="StringList"><default>a,b</default></entry>
  <entry name="icon" type="String"><default>start-here-kde-symbolic</default></entry>
  <entry name="pin" type="Bool"><default>false</default></entry>
 </group></kcfg>
```
- Access with `Plasmoid.configuration.names` (assign a JS array to write).
- `Plasmoid.configuration.valueChanged(key, value)` exists (PD kickoff `LeaveButtons.qml:82`), and per-key `onNamesChanged` signals exist.

**`contents/config/config.qml`** and a config page (PD `applets/kickoff/config.qml`, `ConfigGeneral.qml:24-37`):
```qml
import org.kde.plasma.configuration as PlasmaConfig
PlasmaConfig.ConfigModel { PlasmaConfig.ConfigCategory { name: i18n("General"); icon: "configure"; source: "ConfigGeneral.qml" } }
// ConfigGeneral.qml: KCM.SimpleKCM (import org.kde.kcmutils as KCM)
//   property alias cfg_pin: pinBox.checked ; property var cfg_names ; (cfg_<key>Default also available)
```
`ConfigCategory` properties: name, icon, source, pluginName, visible.

**`main.qml`** (adapted from PD `kickoff/main.qml:26-34,189-198,356-366`, BD `src/applet/qml/main.qml:39,95-117`, PW `mediacontroller/ExpandedRepresentation.qml:26-31`, `digital-clock/main.qml:180`):
```qml
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PC3
import org.kde.kirigami as Kirigami
PlasmoidItem {
    id: root
    preferredRepresentation: compactRepresentation      // always popup
    switchWidth: Kirigami.Units.gridUnit * 10           // inline full rep when bigger
    switchHeight: Kirigami.Units.gridUnit * 10
    toolTipMainText: "Hello"
    Plasmoid.icon: Plasmoid.configuration.icon
    Plasmoid.status: PlasmaCore.Types.ActiveStatus      // Passive = hidden in the tray
    hideOnWindowDeactivate: !Plasmoid.configuration.pin
    compactRepresentation: MouseArea {
        onClicked: root.expanded = !root.expanded
        Kirigami.Icon { anchors.fill: parent; source: Plasmoid.icon }
    }
    fullRepresentation: ColumnLayout {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 14
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20     // popup sizing
        Layout.preferredHeight: Kirigami.Units.gridUnit * 20
        Layout.maximumWidth: Kirigami.Units.gridUnit * 40
        PC3.Label { text: Plasmoid.configuration.names.join(", ") }
    }
    Plasmoid.onSecondaryActivated: console.log("middle click")  // signals: activated, secondaryActivated
    Plasmoid.contextualActions: [
        PlasmaCore.Action { text: "Reset"; icon.name: "edit-clear"; checkable: false
                            priority: PlasmaCore.Action.LowPriority
                            onTriggered: Plasmoid.configuration.names = [] } ]
}
```
- `PlasmoidItem` / AppletQuickItem properties:
  - `expanded`, `switchWidth`, `switchHeight`
  - `compactRepresentation`, `fullRepresentation`, `preferredRepresentation` (QQmlComponent)
  - `compactRepresentationItem`, `fullRepresentationItem`
  - `preloadFullRepresentation`, `activationTogglesExpanded`, `hideOnWindowDeactivate`, `expandedOnDragHover`
  - `badgeText`, `toolTipMainText`, `toolTipSubText`, `toolTipTextFormat`, `toolTipItem`
  - `screen`, `screenGeometry`, `availableScreenRect`
  - signal `externalData`
- `Plasmoid` attached object (Plasma::Applet):
  - read-only: `id`, `formFactor`, `location`, `userConfiguring`, `containment`, `metaData`, `configuration`, `effectiveBackgroundHints`
  - read/write: `title`, `icon`, `status`, `busy`, `backgroundHints`, `globalShortcut`, `configurationRequired`, `hasConfigurationInterface`, `immutability`, `contextualActions`
  - methods: `setInternalAction("configure", action)`, `internalAction(name)`, `removeInternalAction(name)`
- Enums:
  - `PlasmaCore.Types.FormFactor`: Planar, MediaCenter, Horizontal, Vertical, Application
  - `PlasmaCore.Types.Location`: Floating, Desktop, FullScreen, TopEdge, BottomEdge, LeftEdge, RightEdge
  - `PlasmaCore.Types.ItemStatus`: Unknown, Passive, Active, NeedsAttention, RequiresAttention, AcceptingInput, Hidden
  - `PlasmaCore.Types.BackgroundHints`: NoBackground, StandardBackground, TranslucentBackground, ShadowBackground, ConfigurableBackground, DefaultBackground
- `PlasmaCore.Action` is a QAction: `text`, `icon.name`, `checkable`, `checked`, `visible`, `enabled`, `priority` (Low/Normal/HighPriority), `shortcut`, `onTriggered: checked => ...`.

**Custom popup anchored to the panel: `PlasmaCore.Dialog`** (KDE addons `applets/quicklaunch/main.qml:117-129`):
```qml
PlasmaCore.Dialog { id: popup
    type: PlasmaCore.Dialog.PopupMenu            // enum WindowType: Normal, Dock, DialogWindow, PopupMenu, Tooltip, Notification, OnScreenDisplay, CriticalNotification, AppletPopup
    flags: Qt.WindowStaysOnTopHint
    hideOnWindowDeactivate: true
    location: Plasmoid.location
    visualParent: root
    mainItem: Item { implicitWidth: 300; implicitHeight: 200 }   // also: backgroundHints, outputOnly, margins, inset, floating
}  // popup.visible = !popup.visible
```

**Newer `PlasmaCore.PopupPlasmaWindow`** (PD `taskmanager/qml/GroupDialog.qml:18-60`):
```qml
PlasmaCore.PopupPlasmaWindow { id: dlg      // props: visualParent, popupDirection(Qt.Edge), effectivePopupDirection, margin, animated,
    animated: true                           // floating, hideOnWindowDeactivate, removeBorderStrategy (PlasmaCore.AppletPopup.AtScreenEdges|AtPanelEdges)
    popupDirection: Plasmoid.location === PlasmaCore.Types.TopEdge ? Qt.BottomEdge : Qt.TopEdge
    width: content.implicitWidth + leftPadding + rightPadding    // padding props inherited from PlasmaWindow
    mainItem: Item { id: content }
    Component.onCompleted: visible = true    // set visualParent BEFORE making visible
}
```

---
## 2. Kicker / start-menu models and session

**Imports:**
- `org.kde.plasma.private.kicker as Kicker` (exports at 0.0, so import versionless)
- `org.kde.coreaddons as KCoreAddons`
- `org.kde.plasma.private.sessions`
- `org.kde.kirigamiaddons.components as KirigamiComponents`

**Types:**
- RootModel (extends AppsModel)
- AppsModel
- RunnerModel
- KAStatsFavoritesModel (what `RootModel.favoritesModel` returns)
- SimpleFavoritesModel
- RecentUsageModel
- SystemModel
- ComputerModel
- FunnelModel
- ProcessRunner
- DragHelper
- WindowSystem
- SubMenu
- DashboardWindow

```qml
// PD applets/kickoff/main.qml:51-114 (adapted)
Kicker.RootModel { id: rootModel
    autoPopulate: false; appletInterface: root          // root = PlasmoidItem
    flat: true; sorted: true; showSeparators: false; showRootSeparator: true; showTopLevelItems: true
    showAllApps: true; showAllAppsCategorized: false; showFavoritesPlaceholder: true
    showRecentApps: false; showRecentDocs: false; showRecentFolders: false; showPowerSession: false
    appNameFormat: 0           // 0 name, 1 generic, 2 name+generic, 3 generic+name
    Component.onCompleted: favoritesModel.initForClient("my.applet.favorites.instance-" + Plasmoid.id)   // required
}
Kicker.RunnerModel { id: runner; appletInterface: root; mergeResults: true
    favoritesModel: rootModel.favoritesModel; query: searchField.text
    /* optional: runners: ["krunner_services","calculator",...] (PD kicker/main.qml:145-160) */ }
Kicker.RecentUsageModel { id: recent; shownItems: Kicker.RecentUsageModel.OnlyDocs   // AppsAndDocs, OnlyApps, OnlyDocs, OnlyFolders
    ordering: 0 /* Recent; 1 = Popular */; favoritesModel: rootModel.favoritesModel }
Kicker.SystemModel { id: sys; favoritesModel: rootModel.systemFavoritesModel }
ListView { model: rootModel.modelForRow(1)      // row order is [recent*, favorites placeholder, all apps, categories...]; kickoff uses row 1 (ApplicationsPage.qml:148-151)
    delegate: ItemDelegate { required property var model; required property int index
        text: model.display; icon.name: model.decoration
        onClicked: ListView.view.model.trigger(index, "", null) } }   // returns bool = launched
```
- **Roles** (PW `applets/kicker/abstractmodel.cpp:17-37`): display, decoration, group, compactName, description, favoriteId, isParent, isSeparator, hasChildren, hasActionList, actionList, url, disabled, isMultilineText, displayWrapped, compactNameWrapped, isNewlyInstalled. Call `modelForRow(row)` on rows where `isParent` is true to get the child model.
- **Item actions:** `trigger(row, actionId, argument)` with `actionId` taken from the `actionList` entries.
- **Search:** kickoff is KRunner-based. Set `RunnerModel.query` and read `count`, `querying`, `resultsPresent`. With `mergeResults` the results are `modelForRow(0)`, with roles including `group` (kickoff `FullRepresentation.qml:107`). Also available: `startQuery()`, `clear()`, `runners`, `requestUpdateQuery(query)`.
- **Favorites model API** (KAStats): `isFavorite(id)`, `addFavorite(id[,index])`, `removeFavorite(id)`, `moveRow(from,to)`, `favorites` (QStringList), `count`, `maxFavorites`, `enabled`, `addFavoriteTo(id, ":current"|activityId, idx)`, `removeFavoriteFrom`, `setFavoriteOn`, `linkedActivitiesFor`, `portOldFavorites(ids)`, `refresh()`.
- **No-KAStats alternative:** `Kicker.SimpleFavoritesModel { favorites: Plasmoid.configuration.x }` also has `dropPlaceholderIndex` (PD kicker/main.qml:34-38).
- **Context-menu entries for add/remove:** `{text, icon, actionId:"_kicker_favorite_add"|"_kicker_favorite_remove", actionArgument:{favoriteModel, favoriteId}}` (PD `kickoff/code/tools.js:21-45`).
- **Drag to reorder:**
  - Reorder: a `DropArea` calling `view.model.moveRow(source.index, targetIndex)` in `onPositionChanged`, and `addFavoriteTo(url,":current",idx)` for external drops (PD `kickoff/KickoffDropArea.qml:23-60`).
  - Start the drag: `Drag.dragType: Drag.Automatic`, `Drag.mimeData: {"text/uri-list":[url], "favoritedrag":""}` (PD `kicker/ItemGridDelegate.qml:50-58`).
  - `Kicker.DragHelper { startDrag(item,url,icon) }` is for submenus.
- **User and avatar** (PD `kickoff/Header.qml:36,78-93`):
```qml
KCoreAddons.KUser { id: kuser }   // fullName, loginName, faceIconUrl (url), os, host (RO)
KirigamiComponents.AvatarButton { name: kuser.fullName; source: kuser.faceIconUrl; cache: false }
```
- **Power / session** (PW `applets/lock_logout/main.qml:69,179`, `data.js`):
```qml
SessionManagement { id: session }
// session.canLock canLogout canShutdown canReboot canSuspend canHibernate canSuspendThenHibernate canSwitchUser canSaveSession state(Loading|Ready|Error)
session.lock(); session.requestLogout(); session.requestShutdown(); session.requestReboot()  // optional arg SessionManagement.ConfirmationMode.Default|Skip|ForcePrompt
session.suspend(); session.hibernate(); session.hybridSuspend(); session.suspendThenHibernate(); session.switchUser(); session.requestLogoutPrompt(); session.saveSession()
// signals: aboutToSuspend, resumingFromSuspend
```
- **Fast-user-switch list:** `SessionsModel` (roles NameRole, RealNameRole, IconRole, IconNameRole, DisplayNumberRole, VtNumberRole, SessionRole, IsTtyRole; `switchUser(vt)`, `startNewSession()`, `showNewSessionEntry`).
- **Alternative to SessionManagement:** `Kicker.SystemModel.trigger(row,"",null)`, with `favoriteId` in `lock-screen|logout|save-session|switch-user|suspend|hibernate|reboot|shutdown` (PW `kicker/systementry.cpp:39-54`; used in PD `kickoff/LeaveButtons.qml:36-72`).

---
## 3. Notifications (`import org.kde.notificationmanager as NotificationManager`, exports "254.0")
Types: `Notifications` (history/popup model), `Settings`, `Server` (singleton), `ServerInfo`, `Job`, `WatchedNotificationsModel`.

```qml
// PW applets/notifications/main.qml:170-205 (adapted)
NotificationManager.Settings { id: nset }
NotificationManager.Notifications { id: hist
    showExpired: true; showDismissed: true; showJobs: nset.jobsInNotifications
    sortMode: NotificationManager.Notifications.SortByDate         // SortByTypeAndUrgency
    groupMode: NotificationManager.Notifications.GroupApplicationsFlat   // GroupDisabled
    urgencies: NotificationManager.Notifications.CriticalUrgency | NotificationManager.Notifications.NormalUrgency
    blacklistedDesktopEntries: nset.historyBlacklistedApplications
}
hist.count; hist.unreadNotificationsCount; hist.activeNotificationsCount; hist.expiredNotificationsCount
hist.dismissedResidentNotificationsCount; hist.activeJobsCount; hist.jobsPercentage; hist.lastRead = undefined   // "mark all read" = now
hist.clear(NotificationManager.Notifications.ClearExpired)        // "Clear all" (main.qml:224)
const idx = hist.index(row, 0)
hist.close(idx); hist.expire(idx); hist.configure(idx)
hist.invokeDefaultAction(idx, NotificationManager.Notifications.Close)   // behavior: None | Close
hist.invokeAction(idx, actionId, NotificationManager.Notifications.Close); hist.reply(idx, text, behavior)
hist.data(idx, NotificationManager.Notifications.ReadRole)       // setData(idx, true, ReadRole) marks read
```
- **Delegate role names:** the enum `FooRole` becomes `foo`, with `IdRole` becoming `notificationId` (PW `libnotificationmanager/utils.cpp:28-55`). The roles are: notificationId, summary, image, isGroup, groupChildrenCount, expandedGroupChildrenCount, isGroupExpanded, isInGroup, type (NoType|NotificationType|JobType), created, updated, body, iconName, desktopEntry, notifyRcName, applicationName, applicationIconName, originName, jobState, percentage, jobError, suspendable, killable, jobDetails, actionNames, actionLabels, hasDefaultAction, defaultActionLabel, urls, urgency, timeout, configurable, configureActionLabel, closable, expired, dismissed, read, userActionFeedback, hasReplyAction, replyActionLabel, replyPlaceholderText, replySubmitButtonText, replySubmitButtonIconName, category, resident, transient, wasAddedDuringInhibition, hints, dismissable, plus display, decoration, accessibleDescription.
- **DND (the stock applet's approach)**, from `applets/notifications/global/Globals.qml:783-796`:
```qml
function toggleDnd() {
  if (isDnd) { nset.notificationsInhibitedUntil = undefined; nset.revokeApplicationInhibitions(); nset.save() }
  else { const d = new Date(); d.setFullYear(d.getFullYear() + 1); nset.notificationsInhibitedUntil = d; nset.save() }
}
readonly property bool isDnd: !isNaN(nset.notificationsInhibitedUntil.getTime()) && Date.now() < nset.notificationsInhibitedUntil.getTime()
```
  - `Settings` also has `notificationsInhibitedByApplication` (RO), `inhibitNotificationsWhenFullscreen`, `inhibitNotificationsWhenScreensMirrored`, `popupPosition`, `criticalPopupsInDoNotDisturbMode`, `lowPriorityPopups`, `live`, `load()`, `save()`, `defaults()`, `applicationBehavior(entry)`.
  - Popup suppression is applied by the stock notifications applet's private `Globals` singleton (`onNotificationsInhibitedUntilChanged → checkInhibition` sets `NotificationManager.Server.inhibited`, Globals.qml:36-38,466). It needs the stock notifications applet loaded, which it normally is in the tray. Third-party precedent: KdeControlStation's `DndButton.qml` / `js/funcs.js`.
- **`Server`** singleton: `valid`, `inhibited` (writable), `currentOwner`; signals `notificationAdded/Replaced/Removed`.
- Models only work inside the plasmashell process, because the notification server lives there.

---
## 4. Quick-settings sources

### 4a. Network (`org.kde.plasma.networkmanagement as PlasmaNM`; also `org.kde.networkmanager as NMQt` for `NMQt.NetworkManager.Portal`)
```qml
// NM applet/main.qml:76-110,133-160; ConnectionItem.qml:30,271-290
PlasmaNM.EnabledConnections { id: en }   // networkingEnabled wirelessEnabled wirelessHwEnabled wwanEnabled wwanHwEnabled (RO)
PlasmaNM.AvailableDevices   { id: dev }  // wiredDeviceAvailable wirelessDeviceAvailable modemDeviceAvailable bluetoothDeviceAvailable
PlasmaNM.Handler { id: nm }              // hotspotSupported hotspotActive scanning
PlasmaNM.NetworkStatus { id: st }        // activeConnections(string) connectivity
PlasmaNM.ConnectionIcon { id: ic; connectivity: st.connectivity }   // connectionIcon, connectionTooltipIcon, connecting
nm.enableWireless(bool); nm.enableWwan(bool); nm.enableNetworking(bool); nm.requestScan()
nm.enableAirplaneMode(true); PlasmaNM.Configuration.airplaneModeEnabled = true   // do both (singleton "Configuration")
PlasmaNM.NetworkModel { id: net }  PlasmaNM.AppletProxyModel { id: list; sourceModel: net }  // use as ListView model
// capitalized roles: Name Ssid Signal SecurityType SecurityTypeString ConnectionIcon ConnectionPath ConnectionState DevicePath DeviceName SpecificPath
//   Section Type ItemType Uuid Uni LastUsed Slave VpnState VpnType RxBytes TxBytes Duplicate TimeStamp (NM libs/models/networkmodel.cpp:134-162)
nm.activateConnection(model.ConnectionPath, model.DevicePath, model.SpecificPath)       // if model.Uuid present
nm.addAndActivateConnection(model.DevicePath, model.SpecificPath[, password])           // new Wi-Fi network
nm.deactivateConnection(model.ConnectionPath, model.DevicePath)
// model.ConnectionState === PlasmaNM.Enums.Activated | Activating | Deactivated | Deactivating
```
Stock sets `NetworkModel.delayModelUpdates`; gate scanning on the popup being open (a 10.2 s `requestScan()` timer, main.qml:155-160).

### 4b. Bluetooth (`org.kde.bluezqt as BluezQt`, plus `org.kde.plasma.private.bluetooth as PlasmaBt` for the stock device-list model)
```qml
// BD src/applet/qml/main.qml:39-56,95-103; FullRepresentation.qml:49-53; DeviceItem.qml:332-336
BluezQt.Manager.bluetoothOperational; BluezQt.Manager.bluetoothBlocked   // singleton
BluezQt.Manager.adapters; .usableAdapter; .devices; .connectedDevices      // adapter: powered, discoverable, name, startDiscovery()
function setBt(on) { BluezQt.Manager.bluetoothBlocked = !on; BluezQt.Manager.adapters.forEach(a => a.powered = on) }
// device: name address icon connected paired trusted blocked battery?.percentage; connectToDevice() disconnectFromDevice()
PlasmaBt.DevicesProxyModel { id: devs; hideBlockedDevices: true; sourceModel: PlasmaBt.SharedDevicesStateProxyModel }
// roles: Name RemoteName DeviceFullName Address Icon Connected Section Device Connecting Disconnecting ConnectionFailed
// BluezQt.DevicesModel {} is the plain public model
```

### 4c. Volume (`org.kde.plasma.private.volume`, at 0.0; `PulseObjectFilterModel` is 0.1)
```qml
// PA applet/main.qml:26-28,84,132-134,159-165; ListItemBase.qml:423; KdeControlStation components/Volume.qml
GlobalConfig { id: cfg }            // volumeStep, raiseMaximumVolume, audioFeedback, volumeOsd, globalMuteSinks...
readonly property var sink: PreferredDevice.sink         // singleton; .source too; null-check, name "auto_null" = dummy
sink.volume = Math.round(pct / 100 * PulseAudio.NormalVolume)   // writable qlonglong; PulseAudio.{NormalVolume=65536, MinimalVolume, MaximalVolume=1.5x}
sink.muted = !sink.muted
const pct = Math.round(sink.volume / PulseAudio.NormalVolume * 100)
max = cfg.raiseMaximumVolume ? PulseAudio.MaximalVolume : PulseAudio.NormalVolume
SinkModel { id: sinks }  PulseObjectFilterModel { sourceModel: sinks; filterOutInactiveDevices: true }
// roles: PulseObject Name Description Ports ActivePortIndex Index ...;  model.PulseObject.default = true  selects the default
AudioIcon.forVolume(pct, muted, "")   GlobalService.globalMuteSinks() / volumeUp() / volumeDown()
// also Server.defaultSink/defaultSource; SourceModel SinkInputModel SourceOutputModel CardModel; VolumeMonitor{target: sink}
```

### 4d. Battery and power profiles
- **Preferred: private modules**
  - `org.kde.plasma.private.battery` → `BatteryControlModel`. Properties: `hasBatteries`, `hasInternalBatteries`, `hasCumulative`, `pluggedIn`, `state` (BatteryControlModel.NoCharge|Charging|Discharging|FullyCharged), `percent`, `remainingMsec`, `smoothedRemainingMsec`, `chargeStopThreshold`. Roles: PrettyName, Percent, Capacity, Energy, PluggedIn, IsPowerSupply, ChargeState, Type.
  - `org.kde.plasma.private.batterymonitor` → `PowerProfilesControl` and `InhibitionControl` (PDV `applets/batterymonitor/main.qml:28-48,82-92`):
```qml
BatteryControlModel { id: bat }                                   // bat.percent, bat.state === BatteryControlModel.Charging
PowerProfilesControl { id: pp }   // isPowerProfileDaemonInstalled, profiles(QStringList), activeProfile, configuredProfile, profileHolds, profileError, degradationReason, inhibitionReason, isSilent
pp.setProfile("performance")      // "power-saver" | "balanced" | "performance"
InhibitionControl { id: inh }     // hasInhibition, isManuallyInhibited, isLidPresent, triggersLidAction, isSilent; inh.inhibit("reason"); inh.uninhibit()
```
- **Alternative: the `powermanagement` data engine** (P5S `dataengines/powermanagement/powermanagementengine.cpp`):
```qml
import org.kde.plasma.plasma5support as P5S
P5S.DataSource { id: pm; engine: "powermanagement"; connectedSources: ["Battery","AC Adapter","Power Profiles"] }
pm.data["Battery"]["Percent"]; ["State"]  // "Charging"|"Discharging"|"FullyCharged"|"NoCharge"|"Unknown"; ["Has Battery"]
pm.data["AC Adapter"]["Plugged in"]; pm.data["Power Profiles"]["Current Profile"|"Profiles"|"Installed"|"Configured Profile"]
const s = pm.serviceForSource("PowerManagement"); const op = s.operationDescription("setPowerProfile"); op.profile = "balanced"; s.startOperationCall(op)
// other ops (powermanagementjob.cpp): beginSuppressingSleep(reason), stopSuppressingSleep, beginSuppressingScreenPowerManagement, stopSuppressingScreenPowerManagement
```
  - Other sources: "Sleep States", "PowerDevil", "Inhibitions".

### 4e. Brightness (`org.kde.plasma.private.brightnesscontrolplugin`; PDV `applets/brightness/main.qml:38-45`, `PopupDialog.qml:57,91,135-144`)
```qml
ScreenBrightnessControl { id: sb; isSilent: false }   // isBrightnessAvailable, displays (model: displayName label isInternal brightness maxBrightness)
sb.setBrightness(displayName, rawValue)               // raw units, 0..maxBrightness
sb.adjustBrightnessRatio(0.05); sb.adjustBrightnessStep(ScreenBrightnessControl.Increase)   // Decrease, IncreaseSmall, DecreaseSmall
KeyboardBrightnessControl { id: kb }                  // brightness (rw), brightnessMax, isBrightnessAvailable, isSilent
// same module also has DarkModeControl (darkMode, currentTheme, otherTheme), KeyboardColorControl (supported, enabled)
```

### 4f. Night Light (PDV `applets/brightness/NightLightItem.qml:35-66`)
```qml
import org.kde.plasma.workspace.dbus as DBus
import org.kde.plasma.private.brightnesscontrolplugin
DBus.Properties { id: nl; busType: DBus.BusType.Session
    service: "org.kde.KWin.NightLight"; path: "/org/kde/KWin/NightLight"; iface: "org.kde.KWin.NightLight"
    readonly property bool available: Boolean(properties.available)    // also enabled, running, inhibited, mode, daylight, currentTemperature, targetTemperature
}
NightLightInhibitor.toggleInhibition(); NightLightInhibitor.inhibited
```
- `NightLightInhibitor` is a singleton and releases the inhibition on destruction.
- Raw D-Bus methods are `inhibit()` → cookie, `uninhibit(cookie)`, `preview(temp)`, `stopPreview()` (KW `src/plugins/nightlight/org.kde.KWin.NightLight.xml`).
- Generic calls: `DBus.SessionBus.asyncCall({service,path,iface,member,arguments:[new DBus.int32(1)],signature:"i"}, resolve, reject)` (PW `components/dbus/autotests/dbusmethodcalltest.qml:153`).

### 4g. Media (`org.kde.plasma.private.mpris as Mpris`; PW `applets/mediacontroller/main.qml:26-45,167-198,212`)
```qml
Mpris.Mpris2Model { id: mp }                         // mp.currentPlayer (PlayerContainer, may be null), mp.currentIndex (rw)
const p = mp.currentPlayer
p?.track; p?.artist; p?.album; p?.artUrl; p?.identity; p?.playbackStatus === Mpris.PlaybackStatus.Playing   // Stopped, Paused
p?.canPlay canPause canGoNext canGoPrevious canControl canSeek; p?.position (rw), length, volume (rw), shuffle, loopStatus, rate
p.PlayPause(); p.Play(); p.Pause(); p.Next(); p.Previous(); p.Stop(); p.Seek(off); p.Raise(); p.updatePosition(); p.changeVolume(delta, showOSD)
Mpris.MultiplexerModel {}   // single "best player" list; delegate uses model.container.PlayPause() (lockscreen/MediaControls.qml:22-113)
```
- `PlayerContainer` also has `desktopEntry`, `iconName`, `kdePid`, `canRaise`, `canQuit`, `fullscreen`, `hasTrackList`.
- `Mpris2Model.playerForLauncherUrl(url, pid)` finds a player for a given launcher.

### 4h. Do Not Disturb
See §3, or `Settings.notificationsInhibitedUntil` plus `save()`.

---
## 5. System tray items
- **How the stock tray gets them:**
  - `SystemTray` has `Plasmoid.systemTrayModel`, a sorted aggregate of PlasmoidModel, StatusNotifierModel and BackgroundAppsModel (PW `systemtray/systemtray.cpp:387`, `qml/main.qml:58-75`).
  - `StatusNotifierModel` uses the singleton `StatusNotifierItemHost::self()` over D-Bus (`systemtraymodel.cpp:465-475`).
- **Importability:** `org.kde.plasma.private.systemtray` exports `StatusNotifierModel` (QML_ELEMENT, marked "exported to QML for plasma-mobile", `systemtraymodel.h:105-134`). Its default constructor works with no settings, so all items are shown (`systemtraymodel.cpp:32-42`). Technically importable by third parties. Runtime behaviour of a standalone instance is UNVERIFIED, since I could not run it.
- **Roles from StatusNotifierModel** (`systemtraymodel.cpp:399-420`):
  - Base roles (lowercase): `displayText`, `decoration`, `itemType` ("StatusNotifier"), `itemId`, `canRender`, `category`, `status` (`PlasmaCore.Types.PassiveStatus|ActiveStatus|NeedsAttentionStatus|UnknownStatus`), `effectiveStatus`.
  - SNI roles (Capitalized): `DataEngineSource` (the "service/path" key), `Id`, `Title`, `Category` (SNI string), `Status` (enum), `Icon` (QIcon, covers pixmap and theme-path icons), `IconName`, `IconThemePath`, `AttentionIcon`, `AttentionIconName`, `AttentionMovieName`, `OverlayIconName`, `ItemIsMenu`, `ToolTipTitle`, `ToolTipSubTitle`, `WindowId`.
- **Icon source** (PW `qml/StatusNotifierItem.qml:35-47`):
```qml
Kirigami.Icon { source: model.status === PlasmaCore.Types.NeedsAttentionStatus && (model.AttentionIcon || model.AttentionIconName)
                        ? (model.AttentionIcon || model.AttentionIconName) : (model.Icon || model.IconName) }
```
- **Left/middle/right click and scroll** (`StatusNotifierItem.qml:49-90`) call `Plasmoid.activate(src,pos,item)`, `Plasmoid.secondaryActivate`, `Plasmoid.openContextMenu`, `Plasmoid.scroll(src,delta,"vertical"|"horizontal")` and `Plasmoid.popupPosition`. These are `Q_INVOKABLE`s on the C++ `SystemTray` applet (`systemtray.h:51-79`, `systemtray.cpp:324,504-593`), so they **only exist inside the stock tray**. A third-party plasmoid cannot call them.
  - The context menu is a native `QMenu` built by `DBusMenuImporter` and shown by the tray itself. There is no QML dbusmenu module installed. Reimplementing the menu via `com.canonical.dbusmenu` calls is UNVERIFIED.
- **DIY from a third-party plasmoid:** split `DataEngineSource` at the first `/` into service and path (`statusnotifieritemsource.cpp:92-100`), then call `org.kde.StatusNotifierItem` directly:
```qml
function sni(member, args, sig) { const i = src.indexOf("/");
  DBus.SessionBus.asyncCall({service: src.slice(0,i), path: src.slice(i), iface: "org.kde.StatusNotifierItem", member, arguments: args, signature: sig}) }
sni("Activate", [new DBus.int32(x), new DBus.int32(y)], "ii")     // left click, x/y = global screen coords
sni("SecondaryActivate", [..], "ii")                              // middle
sni("ContextMenu", [..], "ii")                                    // right; only works for items without a dbusmenu "Menu"; stock calls it only as a fallback (statusnotifieritemsource.cpp:603-613)
sni("Scroll", [new DBus.int32(delta), new DBus.string("vertical")], "is")
```
  - The stock tray also passes an xdg-activation token first (`ProvideXdgActivationToken`; `systemtray.cpp:528-531`). On Wayland, skipping it may stop apps from raising windows. UNVERIFIED.
  - Tray settings live in the applet's `[General]` config group (see §9).

---
## 6. Keyboard layout (`org.kde.plasma.workspace.keyboardlayout`, version 1.0; PD `applets/keyboardlayout/qml/main.qml`; installed `org/kde/plasma/workspace/components/KeyboardLayoutSwitcher.qml`)
```qml
import org.kde.plasma.workspace.keyboardlayout
KeyboardLayout { id: kl }          // layout (uint, rw: current index), layoutsList (list of {shortName, displayName, longName}), initialize(), switchToNextLayout(), switchToPreviousLayout()
kl.layoutsList.length > 1; kl.layoutsList[kl.layout].shortName
// ready-made: import org.kde.plasma.workspace.components → KeyboardLayoutSwitcher {}  (MouseArea: click = next, wheel = prev/next, alias keyboardLayout, layoutNames, hasMultipleKeyboardLayouts)
// flag icons: import org.kde.plasma.private.kcm_keyboard as KCMKeyboard; KCMKeyboard.Flags.getIcon(shortName)
```
Also in the module: `KWinVirtualKeyboard` (virtual-keyboard state).

---
## 7. Calendar and clock
```qml
// PW applets/digital-clock/CalendarView.qml:59-61,620-647 (adapted)
import org.kde.plasma.workspace.calendar as PlasmaCalendar
import org.kde.plasma.clock
Clock { id: clk; trackSeconds: false }                 // dateTime, timeZone (QByteArray; "Local" means system), isSystemTimeZone, timeZoneCode/Name/Offset, valid
Clock { id: berlin; timeZone: "Europe/Berlin" }
PlasmaCalendar.EventPluginsManager { id: epm; enabledPlugins: Plasmoid.configuration.enabledCalendarPlugins }   // always create one; MonthView passes it to daysModel.setPluginsManager
PlasmaCalendar.MonthView { anchors.fill: parent
    eventPluginsManager: epm; today: clk.dateTime; showWeekNumbers: false
    firstDayOfWeek: Qt.locale().firstDayOfWeek
    // aliases: selectedMonth, selectedYear, currentIndex, daysModel; props: currentDate, showDate, todayAuxilliaryText, viewHeader, cellHeight
    // functions: resetToToday(), nextView(), previousView(), showMonthView/YearView/DecadeView(); enum CalendarView{DayView,MonthView,YearView}
}
```
- `Calendar` backend type: `displayedDate`, `today`, `weeksModel`, `monthName`, `year`, `month`, `daysModel`, `nextMonth()`, `previousMonth()`, `goToMonth(m)`, `goToYear(y)`, `dayName(i)`, `resetToToday()`.
- `daysModel.eventsForDate(date)` and `agendaUpdated(date)`.
- **Time formatting** (PW `digital-clock/DigitalClock.qml:28-50,434,521-546`):
  - `Qt.locale().toString(clock.dateTime, fmt)`, with `fmt` derived from `Qt.locale().timeFormat(Locale.ShortFormat)`.
  - Rebuild `hh<delim>mm[<delim>ss] [AP]` from the locale format. Lowercase `hh` forces a 12h or 24h clock decision, and `AP` is appended when the locale or the user wants 12h.
  - Date: `Qt.formatDate(d, Qt.locale(), Locale.LongFormat|ShortFormat)` or `Qt.formatDate(d, Qt.ISODate)`.
- **Time zones:**
  - `selectedTimeZones` includes "Local" and uses IANA ids.
  - `Clock.timeZone = "Local"` calls `resetTimeZone()` (PW `libclock/clock.cpp:56-76`).
  - Picker UI: `org.kde.plasma.workspace.timezoneselector` (`TimezoneSelector.qml`).
  - Config helper: `org.kde.plasma.private.digitalclock` (TimeZoneModel and ApplicationIntegration).

---
## 8. Look-and-feel / shell QML entry points (6.8.2)
**Splash**
- File: L&F `contents/splash/Splash.qml` (`splashmainscript`; PW `ksplash/ksplashqml/splashwindow.cpp:90-125`).
- Root is any Item. KSplash writes `rootObject()->setProperty("stage", int)` (`splashwindow.cpp:58-63`).
- Stages go 1 to 6 (`splashapp.cpp:28-36,110-117`): 1 initial, 2 startPlasma, 3 kcminit (dropped), 4 ksmserver, 5 wm, 6 desktop. Stage 6 closes the splash.
- Breeze reference: `/usr/share/plasma/look-and-feel/org.kde.breeze.desktop/contents/splash/Splash.qml` (`property int stage`; at stage 2 fade in, at stage 5 fade out the busy indicator).
- Assets are referenced relatively as `images/...`.

**Lockscreen** (installed `/usr/share/plasma/shells/org.kde.plasma.desktop/contents/lockscreen/{LockScreen,LockScreenUi,MainBlock,MediaControls,NoPasswordUnlock,PasswordSync,config}.qml`, `config.xml`, `qmldir`)
- Root, as in `LockScreen.qml`:
```qml
Item { property bool debug; property string notification; signal clearPassword(); signal notificationRepeated()
       property bool viewVisible: false     // greeter writes "viewVisible" and "locked" on the root (greeterapp.cpp:398,427)
       LockScreenUi { anchors.fill: parent } }
```
- Context objects set by the greeter (KSL `greeter/greeterapp.cpp:328-331,361`):
  - `authenticator` (type `org.kde.kscreenlocker` Authenticators). Properties: `busy`, `inPasswordDelay`, `prompt`, `promptForSecret`, `infoMessage`, `errorMessage`, `unlocked`, `hadPrompt`, `authenticatorTypes`, `state`, `authenticator`. Methods: `startAuthenticating()`, `stopAuthenticating()`, `respond(pw)`, `cancel()`. Signals: `succeeded`, `failed(what, authenticator)`, `noninteractiveError`, `noninteractiveInfo`, `loginFailedDelayStarted`.
  - `kscreenlocker_userName`, `kscreenlocker_userImage`.
  - `config` (KConfigPropertyMap from `config.xml`: `alwaysShowClock`, `hideClockWhenIdle`, `showMediaControls`).
  - `wallpaper` item.
  - `Qt.quit()` is honoured only once `authenticator.unlocked`.
- Single-user model pattern: `ListModel { users }` filled with `{name: kscreenlocker_userName, realName: ..., icon: ...}` (`LockScreenUi.qml:274-281`).
- Imports used: `org.kde.kscreenlocker as ScreenLocker` (`AuthenticatorModel`, `ActiveScreenMonitor`, `LockscreenState`), `org.kde.plasma.private.sessions`, `org.kde.plasma.private.keyboardindicator as KeyboardIndicator` (`KeyState {key: Qt.Key_CapsLock}`), `org.kde.plasma.private.accessibility`, `org.kde.plasma.workspace.keyboardlayout as Keyboards`, and `org.kde.breeze.components` (installed at `/usr/lib/qt6/qml/org/kde/breeze/components`: ActionButton, Battery, Clock, SessionManagementScreen, UserDelegate, UserList, WallpaperFader, RejectPasswordAnimation).
- Oddity: the installed `LockScreenUi.qml` reads `authenticator.pamTimeout`, but KSL v6.8.2 has no such property (`pamauthenticators.h`). It evaluates to undefined.

**Logout** (installed `shells/org.kde.plasma.desktop/contents/logout/{Logout,LogoutControls,LogoutButton}.qml`, `timer.js`, `controlsowner.js`)
- Root `Item` signals the greeter connects to (`shutdowndlg.cpp:235-244`): `logoutRequested()`, `haltRequested()`, `haltUpdateRequested()`, `suspendRequested(int)` (2 = sleep, 4 = hibernate), `rebootRequested()`, `rebootRequested2(int)`, `rebootUpdateRequested()`, `cancelRequested()`, `lockScreenRequested()`, `cancelSoftwareUpdateRequested()`.
- Context properties (`shutdowndlg.cpp:90-212`): `maysd`, `sdtype`, `ShutdownType` (ShutdownTypeDefault|None|Reboot|Halt|Logout), `spdMethods.{StandbyState,SuspendState,HibernateState}`, `canLogout`, `rebootToFirmwareSetup`, `isUefi`, `rebootToBootLoaderMenu`, `rebootToBootLoaderEntry`, `softwareUpdatePending`, `bootManager`, `choose`, `rebootOptions`.
- `Logout.qml` also has `timeout` (default 30), `remainingTime` (counts down when `sdtype != Default`), and runs `currentAction()` at 0.

**OSD**
- Module `org.kde.plasma.workspace.osd` (installed at `/usr/lib/qt6/qml/org/kde/plasma/workspace/osd/{Osd,OsdItem}.qml`).
- `Osd` is an `OsdWindow` (PlasmaWindow) with aliases `timeout` (1800), `osdValue`, `osdMaxValue` (100), `osdAdditionalText`, `icon`, `showingProgress`; `mainItem: OsdItem {}` (a RowLayout).
- `shell/osd.cpp:282-300` sets: `osdMaxValue`, `osdValue`, `osdAdditionalText`, `showingProgress` and `icon` (progress), or `showingProgress=false`, `osdValue`=text and `icon` (text). It reads `timeout` once.
- Triggers come over D-Bus `org.kde.plasmashell /org/kde/osdService`.
- Not customizable via L&F in 6.8.2 (see top).

**Window switcher**
- KWin package: `~/.local/share/kwin/tabbox/<id>/{metadata.json,contents/ui/main.qml}`.
  - `metadata.json`: `KPackageStructure: "KWin/WindowSwitcher"`, `KPlugin.Id`, `"X-Plasma-API":"declarativeappletscript"` (`/usr/share/kwin/tabbox/thumbnail_grid/metadata.json`).
  - Installed switchers: big_icons, compact, coverswitch, flipswitch, sidebar, thumbnail_grid.
- The L&F variant is `contents/windowswitcher/WindowSwitcher.qml`. It is selected when `kwinrc [TabBox] LayoutName` equals the L&F id. The L&F `defaults` file maps `[kwinrc][WindowSwitcher] LayoutName=<id>` (PW `libklookandfeel/klookandfeelmanager.cpp:91,123-129,489-492`).
- Root API: `import org.kde.kwin as KWin` is registered in the KWin process (`qmlRegisterType`, 3.0), not an installed module:
```qml
// /usr/share/kwin/tabbox/compact/contents/ui/main.qml; KW src/tabbox/switcheritem.h:25-40, clientmodel.cpp:124-134
KWin.TabBoxSwitcher { id: tabBox   // props: model, screenGeometry(rect), visible(rw), allDesktops, currentIndex(rw), noModifierGrab, compositing, automaticallyHide(rw), item; signals aboutToShow, aboutToHide
    currentIndex: list.currentIndex
    PlasmaCore.Dialog { location: PlasmaCore.Types.Floating; visible: tabBox.visible; flags: Qt.ToolTip | Qt.X11BypassWindowManagerHint
        x: tabBox.screenGeometry.x + tabBox.screenGeometry.width/2 - mainItem.width/2   // y likewise
        mainItem: ListView { id: list; model: tabBox.model
            delegate: Item { /* roles: caption desktopName minimized windowId closeable icon */
                KWin.WindowThumbnail { wId: windowId }     // needs a container item
                TapHandler { onTapped: tabBox.model.activate(index) } } } } }
// model methods: activate(int), close(int), longestCaption()
```

---
## 9. Panel layout script (JS)
Files: L&F `contents/layouts/org.kde.plasma.desktop-layout.js` (installed Breeze just calls `loadTemplate("org.kde.plasma.desktop.defaultPanel")`), and the templates in `/usr/share/plasma/layout-templates/<id>/contents/layout.js`. Reference: PD's default template, and the Klassy one `/usr/share/plasma/layout-templates/org.kde.klassy.plasma.desktop.bottomPanel/contents/layout.js`.

**Globals** (PW `shell/scripting/scriptengine.cpp:100-200`, `scriptengine_v1.h`):
- `Panel` (constructor), `ConfigFile(file, group)`, `loadTemplate(id)`, `loadSerializedLayout(json)`
- `desktops()`, `desktopsForActivity(currentActivity())`, `desktopById`, `desktopForScreen`, `panels()`, `panelById`, `activities()`, `createActivity`
- `screenGeometry(screen)`, `gridUnit`, `languageId`, `locale`, `hasBattery`, `screenCount`, `knownWidgetTypes`, `theme`
- `applicationExists`, `defaultApplication`, `fileExists`, `userDataPath`, `print`, `sleep`, `getApiVersion(1)`

**Panel properties** (`panel.h:22-47`; value strings from `panel.cpp`):
- `location`: top | bottom | left | right | floating | desktop | fullscreen. This also sets `formFactor` ("horizontal" | "vertical").
- `height` (thickness)
- `alignment`: left | center | right (`panel.cpp:145-158`)
- `offset`, `length`, `minimumLength`, `maximumLength`
- `lengthMode`: fill | fit | custom. Anything else, including "manual" as in CachyOS-Nord, silently means fill (`panel.cpp:192-203`).
- `hiding`: none | autohide | dodgewindows | windowsgobelow (`panel.cpp:305-336`). The "normal" value that Klassy and CachyOS-Nord write also resolves to none.
- `floating` (bool), `floatingApplets` (bool), `opacity`: adaptive | opaque | translucent, `screen` (int), `formFactor` (RO), `widgetIds`, `widgets(type?)`, `addWidget(...)`, `remove()`, `showConfigurationInterface()`.

**Widgets.** `addWidget(pluginId)` returns a widget object (`containment.cpp:164-218`; extra args x, y, w, h, args). Widget methods:
- `readConfig(key, def)`, `writeConfig(key, value)`, `reloadConfig()`
- `currentConfigGroup = ["General"]`, which means `[Containments][N][Applets][M][Configuration][General]` (`applet.cpp:49-62`; LP `applet.cpp:203`)
- `globalShortcut = "Alt+F1"`, `index`, `locked`, `geometry`, `type`, `id`, `configKeys`, `configGroups`, `remove()`
- Call `reloadConfig()` between writes if values don't apply (Klassy comment).

```js
var panel = new Panel;
panel.location = "bottom"; panel.height = 2 * Math.floor(gridUnit * 1.8 / 2);
panel.lengthMode = "custom"; panel.minimumLength = panel.maximumLength = 1000; panel.alignment = "center";
panel.floating = true; panel.hiding = "none"; panel.opacity = "adaptive";

var kick = panel.addWidget("org.kde.plasma.kickoff");
kick.currentConfigGroup = ["General"]; kick.writeConfig("icon", "start-here-kde-symbolic"); kick.reloadConfig();
kick.globalShortcut = "Alt+F1";

panel.addWidget("org.kde.plasma.panelspacer");            // config General: expanding (bool, default true), length (int, default -1) - PW panelspacer/main.xml
var tm = panel.addWidget("org.kde.plasma.icontasks");
tm.currentConfigGroup = ["General"];
tm.writeConfig("launchers", "applications:org.kde.dolphin.desktop,preferred://browser");   // StringList key; comma-joined string as in Klassy
tm.reloadConfig();                                         // default: applications:systemsettings.desktop,applications:org.kde.discover.desktop,preferred://filemanager,preferred://browser
panel.addWidget("org.kde.plasma.panelspacer");

var tray = panel.addWidget("org.kde.plasma.systemtray");
tray.currentConfigGroup = ["General"];
tray.writeConfig("extraItems", "org.kde.plasma.battery,org.kde.plasma.bluetooth,org.kde.plasma.networkmanagement,org.kde.plasma.volume,org.kde.plasma.notifications");  // plasmoids enabled in the tray
tray.writeConfig("shownItems",  "org.kde.plasma.networkmanagement");   // always in the panel area
tray.writeConfig("hiddenItems", "org.kde.plasma.clipboard");           // always in the popup
tray.writeConfig("showAllItems", false);                               // other keys: disabledStatusNotifiers, knownItems, scaleIconsToFit, iconSpacing, reverseIconOrder
tray.reloadConfig();
var clock = panel.addWidget("org.kde.plasma.digitalclock");
clock.currentConfigGroup = ["Appearance"]; clock.writeConfig("dateFormat", "custom"); clock.writeConfig("customDateFormat", "ddd-d");

for (var d of desktopsForActivity(currentActivity())) d.wallpaperPlugin = "org.kde.image";
var kw = ConfigFile("kwinrc"); kw.group = "Effect-overview"; kw.writeEntry("BorderActivate", "9");
```
- The tray keys are in the tray applet's own `[General]` (PW `systemtray/main.xml:8-48`, `systemtraysettings.cpp:14-16,122`). There is no separate tray containment any more (`SystrayContainmentId` is only a migration key).
- Using JS arrays instead of comma strings with `writeConfig` is UNVERIFIED (the code does `QJSValue::toVariant()`, `applet.cpp:125`).
- The CachyOS-Nord/emerald themes use `currentConfigGroup = ["Shortcuts"]` and `["/Configuration/General"]`. I did not verify these forms work in 6.8.2; use `["General"]` plus the `globalShortcut` property.
- The serialized-layout alternative is a JSON `var layout = {desktops:[...], panels:[{alignment, applets:[{plugin, config:{"/Configuration/General":{...}}}], config:{...}}]}` consumed by `loadSerializedLayout(layout)`. See `/usr/share/plasma/look-and-feel/com.github.rkstrdee.emerald/contents/layouts/org.kde.plasma.desktop-layout.js`.
- Plasmoid-specific setup scripts live in L&F `contents/plasmoidsetupscripts/<pluginId>.js`; Klassy ships one for `org.kde.plasma.battery`. Their API is UNVERIFIED.

---
## Files worth opening
- Stock source: `.../scratchpad/up/plasma-desktop/applets/kickoff/`
- Stock source: `.../scratchpad/up/plasma-workspace/applets/{systemtray,notifications,mediacontroller,digital-clock,lock_logout}/`
- Stock source: `.../scratchpad/up/plasma-nm/applet/`, `.../scratchpad/up/bluedevil/src/applet/qml/`, `.../scratchpad/up/plasma-pa/applet/`, `.../scratchpad/up/powerdevil/applets/{batterymonitor,brightness}/`
- Installed shell package: `/usr/share/plasma/shells/org.kde.plasma.desktop/contents/{lockscreen,logout}/`
- Installed modules: `/usr/lib/qt6/qml/org/kde/plasma/workspace/{osd,calendar,keyboardlayout}/`, `/usr/lib/qt6/qml/org/kde/breeze/components/`
- Installed KWin switchers: `/usr/share/kwin/tabbox/*/contents/ui/main.qml`
- Installed layout templates: `/usr/share/plasma/layout-templates/*/contents/layout.js`
- Third-party working examples (installed): `~/.local/share/plasma/plasmoids/{KdeControlStation,AndromedaLauncher,org.kde.plasma.simplekickoff}`
