/*
 * SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphinplacesmodelsingleton.h"
#include "lumen/lumenicons.h"
#include "lumen/lumennames.h"
#include "trash/dolphintrash.h"
#include "views/draganddrophelper.h"

#include <KAboutData>

#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QMimeData>

DolphinPlacesModel::DolphinPlacesModel(QObject *parent)
    : KFilePlacesModel(parent)
{
    connect(&Trash::instance(), &Trash::emptinessChanged, this, &DolphinPlacesModel::slotTrashEmptinessChanged);
}

DolphinPlacesModel::~DolphinPlacesModel() = default;

bool DolphinPlacesModel::panelsLocked() const
{
    return m_panelsLocked;
}

void DolphinPlacesModel::setPanelsLocked(bool locked)
{
    if (m_panelsLocked == locked) {
        return;
    }

    m_panelsLocked = locked;

    if (rowCount() > 0) {
        int lastPlace = rowCount() - 1;

        for (int i = 0; i < rowCount(); ++i) {
            if (KFilePlacesModel::groupType(index(i, 0)) != KFilePlacesModel::PlacesType) {
                lastPlace = i - 1;
                break;
            }
        }

        Q_EMIT dataChanged(index(0, 0), index(lastPlace, 0), {KFilePlacesModel::GroupRole});
    }
}

QStringList DolphinPlacesModel::mimeTypes() const
{
    QStringList types = KFilePlacesModel::mimeTypes();
    types << DragAndDropHelper::arkDndServiceMimeType() << DragAndDropHelper::arkDndPathMimeType();
    return types;
}

bool DolphinPlacesModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    // We make the view accept the drag by returning them from mimeTypes()
    // but the drop should be handled exclusively by PlacesPanel::slotUrlsDropped
    if (DragAndDropHelper::isArkDndMimeType(data)) {
        return false;
    }

    return KFilePlacesModel::dropMimeData(data, action, row, column, parent);
}

QVariant DolphinPlacesModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        if (isDevice(index)) {
            return Lumen::driveName(KFilePlacesModel::data(index, role).toString());
        }
        break;
    case Qt::DecorationRole:
        if (isTrash(index)) {
            return Lumen::fullColorIcon(QIcon::fromTheme(m_isEmpty ? QStringLiteral("user-trash") : QStringLiteral("user-trash-full")));
        }
        return Lumen::fullColorIcon(KFilePlacesModel::data(index, role).value<QIcon>());
    case KFilePlacesModel::GroupRole: {
        // Windows navigation pane: PlacesPanel paints the titles; the view tells sections apart by label, so each group gets a
        // distinct invisible one. Home on top gets none and stands alone, without a section header.
        if (index.row() == 0 && url(index).matches(QUrl::fromLocalFile(QDir::homePath()), QUrl::StripTrailingSlash)) {
            return QString();
        }
        return QString(int(KFilePlacesModel::groupType(index)) + 1, QChar(0x200B));
    }
    }

    return KFilePlacesModel::data(index, role);
}

void DolphinPlacesModel::slotTrashEmptinessChanged(bool isEmpty)
{
    if (m_isEmpty == isEmpty) {
        return;
    }

    // NOTE Trash::isEmpty() reads the config file whereas emptinessChanged is
    // hooked up to whether a dirlister in trash:/ has any files and they disagree...
    m_isEmpty = isEmpty;

    for (int i = 0; i < rowCount(); ++i) {
        const QModelIndex index = this->index(i, 0);
        if (isTrash(index)) {
            Q_EMIT dataChanged(index, index, {Qt::DecorationRole});
        }
    }
}

bool DolphinPlacesModel::isTrash(const QModelIndex &index) const
{
    return url(index) == QUrl(QStringLiteral("trash:/"));
}

DolphinPlacesModelSingleton::DolphinPlacesModelSingleton()
    : m_placesModel(new DolphinPlacesModel())
{
    // Destroy during QCoreApplication teardown, while KIO's KCoreDirListerCache is still alive.
    qAddPostRoutine([] {
        instance().m_placesModel.reset();
    });
}

DolphinPlacesModelSingleton &DolphinPlacesModelSingleton::instance()
{
    static DolphinPlacesModelSingleton s_self;
    return s_self;
}

DolphinPlacesModel *DolphinPlacesModelSingleton::placesModel() const
{
    return m_placesModel.data();
}

#include "moc_dolphinplacesmodelsingleton.cpp"
