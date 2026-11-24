#pragma once

#include <QIcon>

namespace Lumen
{
/// Some themes (Win11) draw 22/24 px icons as monochrome outlines and only 16 px and 32+ px in color.
/// Sizes between those render the theme's large artwork instead.
QIcon fullColorIcon(const QIcon &icon);
}
