import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

// Sized by its visible artwork (see SystemTray::iconPlacement), so padded and full-bleed icons match.
Kirigami.Icon {
    readonly property int cell: parent ? Math.min(parent.width, parent.height) : 0
    readonly property rect placement: Plasmoid.iconPlacement(source, cell)

    roundToIconSize: false
    x: placement.x
    y: placement.y
    width: placement.width
    height: placement.height
}
