import QtQuick
import org.kde.kirigami.templates as KT

// Windows card: flat fill, hairline stroke, control radius, no shadow.
KT.AbstractCard {
    id: root

    background: Rectangle {
        LumenTokens {
            id: lt
        }

        radius: lt.radiusControl
        color: root.checked || (root.showClickFeedback && (root.down || root.highlighted)) ? lt.controlFillPressed
             : root.hoverEnabled && root.hovered ? lt.controlFillHover
             : lt.cardBg
        border.width: 1
        border.color: lt.cardStroke
    }
}
