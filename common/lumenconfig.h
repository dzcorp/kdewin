#pragma once

#include "lumentokens.h"

#include <QStringList>

namespace Lumen
{
/// User tuning from ~/.config/lumenrc (written by the Lumen System Settings module).
/// Defaults are the design tokens; reload() only re-reads when the file changed.
struct Config {
    // [Decoration]
    bool translucentTitlebar = true;
    int titlebarOpacity = int(T::Surface::TitlebarAlphaActiveDark * 100);
    int windowRadius = int(T::Radius::Window);
    int titlebarHeight = int(T::Size::Titlebar);
    bool centerTitle = false;
    // [Style]
    bool animations = true;
    int animationDuration = 100; // percent of the token durations
    bool scrollbarExpanded = false;
    bool translucentMenus = true;
    bool translucentWindows = true;
    int windowOpacity = 0; // percent of window color painted over Mica
    QStringList opaqueApps;
    int controlRadius = int(T::Radius::Control);
    // [Materials]
    bool materials = true;

    bool operator==(const Config &) const = default;

    int duration(int tokenMs) const
    {
        return animations ? tokenMs * animationDuration / 100 : 0;
    }

    static const Config &get();
    /// Re-read if lumenrc changed on disk. Returns true when values changed.
    static bool reload();
};
}
