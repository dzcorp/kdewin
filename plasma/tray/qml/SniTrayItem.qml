pragma ComponentBehavior: Bound
import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

TrayItem {
    id: sni

    text: model?.Title || model?.ToolTipTitle || ""
    mainText: model?.ToolTipTitle || text
    subText: model?.ToolTipSubTitle ?? ""
    active: mainText !== "" || subText !== ""

    TrayIcon {
        parent: sni.iconContainer
        active: sni.hovered
        source: {
            if (sni.model?.status === PlasmaCore.Types.NeedsAttentionStatus) {
                if (sni.model?.AttentionIcon) {
                    return sni.model.AttentionIcon;
                }
                if (sni.model?.AttentionIconName) {
                    return sni.model.AttentionIconName;
                }
            }
            return sni.model?.Icon || sni.model?.IconName || "";
        }
    }

    function menuPos(mouse) {
        return mouse ? Plasmoid.popupPosition(sni, mouse.x, mouse.y) : Plasmoid.popupPosition(sni, width / 2, height / 2);
    }

    onActivated: pos => {
        if (model.ItemIsMenu) {
            Plasmoid.openContextMenu(model.DataEngineSource, pos, sni);
        } else {
            Plasmoid.activate(model.DataEngineSource, pos, sni);
        }
    }
    onContextMenu: mouse => Plasmoid.openContextMenu(model.DataEngineSource, menuPos(mouse), sni)
    onClicked: mouse => {
        const pos = menuPos(mouse);
        switch (mouse.button) {
        case Qt.LeftButton:
            activated(pos);
            break;
        case Qt.RightButton:
            Plasmoid.openContextMenu(model.DataEngineSource, pos, sni);
            break;
        case Qt.MiddleButton:
            Plasmoid.secondaryActivate(model.DataEngineSource, pos);
            break;
        }
    }
    onWheel: wheel => {
        if (wheel.angleDelta.y !== 0) {
            Plasmoid.scroll(model.DataEngineSource, wheel.angleDelta.y, "vertical");
        }
        if (wheel.angleDelta.x !== 0) {
            Plasmoid.scroll(model.DataEngineSource, wheel.angleDelta.x, "horizontal");
        }
    }
}
