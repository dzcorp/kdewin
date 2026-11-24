use crate::ctx::Ctx;
use crate::tokens::fmt_num;
use anyhow::Result;

pub fn generate(ctx: &Ctx) -> Result<()> {
    let t = &ctx.t;
    let px = |k: &str| format!("{}px", fmt_num(t.f(k)));
    let css = USERCHROME
        .replace("{r_control}", &px("radius.control"))
        .replace("{r_overlay}", &px("radius.overlay"))
        .replace("{menu_item_h}", &px("size.menu_item_h"))
        .replace("{ms_fast}", &format!("{}ms", t.i("motion.fast_ms")));
    ctx.write("apps/firefox/chrome/userChrome.css", css)?;
    ctx.write("apps/firefox/user.js", USERJS)
}

const USERJS: &str = r#"// Lumen: enable userChrome.css and follow the system (GTK/KDE) theme.
user_pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);
user_pref("widget.gtk.overlay-scrollbars.enabled", true);
user_pref("browser.tabs.inTitlebar", 1);
user_pref("widget.use-xdg-desktop-portal.file-picker", 1);
"#;

const USERCHROME: &str = r#"/* Lumen userChrome — geometry only; colors follow the system theme. */
:root {
  --tab-border-radius: {r_overlay} !important;
  --toolbarbutton-border-radius: {r_control} !important;
  --arrowpanel-border-radius: {r_overlay} !important;
  --panel-border-radius: {r_overlay} !important;
  --border-radius-small: {r_control} !important;
  --border-radius-medium: {r_overlay} !important;
  --tab-min-height: 32px !important;
  --tab-block-margin: 4px !important;
}

.tab-background { border-radius: {r_overlay} {r_overlay} 0 0 !important; margin-block: 4px 0 !important; }
.tabbrowser-tab[selected] .tab-background { box-shadow: 0 0 0 1px rgba(0, 0, 0, 0.12) !important; }
.tab-content { padding-inline: 12px !important; }

#urlbar-background, #searchbar { border-radius: {r_control} !important; }
#urlbar[focused] > #urlbar-background { outline-width: 1px !important; box-shadow: inset 0 -2px AccentColor !important; }

menupopup, panel {
  --panel-padding: 4px !important;
  --panel-border-radius: {r_overlay} !important;
}
menupopup > menuitem, menupopup > menu {
  min-height: {menu_item_h} !important;
  padding-inline: 11px !important;
  margin-inline: 4px !important;
  border-radius: {r_control} !important;
}
menupopup > menuseparator { margin-inline: 0 !important; }

.titlebar-buttonbox .titlebar-button {
  width: 46px !important;
  padding: 0 !important;
  border-radius: 0 !important;
  transition: background-color {ms_fast} ease-out !important;
}
.titlebar-buttonbox .titlebar-close:hover { background-color: #c42b1c !important; color: white !important; }
"#;
