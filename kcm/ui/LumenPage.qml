import QtQuick
import QtQuick.Layouts
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    default property alias content: column.data

    ColumnLayout {
        id: column
        spacing: 0
    }
}
