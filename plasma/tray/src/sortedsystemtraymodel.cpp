/*
    SPDX-FileCopyrightText: 2019 Konrad Materka <materka@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sortedsystemtraymodel.h"
#include "debug.h"
#include "systemtraymodel.h"

#include <QList>

static const QList<QString> s_categoryOrder = {
    QStringLiteral("UnknownCategory"),
    QStringLiteral("ApplicationStatus"),
    QStringLiteral("Communications"),
    QStringLiteral("SystemServices"),
    QStringLiteral("Hardware"),
};

SortedSystemTrayModel::SortedSystemTrayModel(SortingType sorting, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_sorting(sorting)
{
    setSortLocaleAware(true);
    sort(0);
}

void SortedSystemTrayModel::setOrder(const QStringList &order)
{
    QHash<QString, int> next;
    next.reserve(order.size());
    for (int i = 0; i < order.size(); ++i) {
        next.insert(order.at(i), i);
    }
    if (next == m_order) {
        return;
    }
    m_order = std::move(next);
    invalidate();
}

bool SortedSystemTrayModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    switch (m_sorting) {
    case SortedSystemTrayModel::SortingType::ConfigurationPage:
        return lessThanConfigurationPage(left, right);
    case SortedSystemTrayModel::SortingType::SystemTray:
        return lessThanSystemTray(left, right);
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

bool SortedSystemTrayModel::lessThanConfigurationPage(const QModelIndex &left, const QModelIndex &right) const
{
    const int categoriesComparison = compareCategoriesAlphabetically(left, right);
    if (categoriesComparison == 0) {
        return QSortFilterProxyModel::lessThan(left, right);
    } else {
        return categoriesComparison < 0;
    }
}

bool SortedSystemTrayModel::lessThanSystemTray(const QModelIndex &left, const QModelIndex &right) const
{
    QVariant itemIdLeft = left.data(static_cast<int>(BaseModel::BaseRole::ItemId));
    QVariant itemIdRight = right.data(static_cast<int>(BaseModel::BaseRole::ItemId));

    // Manual order (drag-sorted by the user) wins; unordered items follow in the stock order.
    if (!m_order.isEmpty()) {
        const int l = m_order.value(itemIdLeft.toString(), -1);
        const int r = m_order.value(itemIdRight.toString(), -1);
        if (l >= 0 || r >= 0) {
            return r < 0 || (l >= 0 && l < r);
        }
    }
    const auto isNotifications = [](const QVariant &id) {
        return id.toString() == QLatin1String("org.kde.plasma.notifications") || id.toString() == QLatin1String("org.lumen.notifier");
    };
    // The notifications item always comes last.
    if (isNotifications(itemIdRight)) {
        return false;
    } else if (isNotifications(itemIdLeft)) {
        return true;
    }

    const int categoriesComparison = compareCategoriesOrderly(left, right);
    if (categoriesComparison == 0) {
        return QSortFilterProxyModel::lessThan(left, right);
    } else {
        return categoriesComparison < 0;
    }
}

int SortedSystemTrayModel::compareCategoriesAlphabetically(const QModelIndex &left, const QModelIndex &right) const
{
    QVariant leftData = left.data(static_cast<int>(BaseModel::BaseRole::Category));
    QString leftCategory = leftData.isNull() ? QStringLiteral("UnknownCategory") : leftData.toString();

    QVariant rightData = right.data(static_cast<int>(BaseModel::BaseRole::Category));
    QString rightCategory = rightData.isNull() ? QStringLiteral("UnknownCategory") : rightData.toString();

    return QString::localeAwareCompare(leftCategory, rightCategory);
}

int SortedSystemTrayModel::compareCategoriesOrderly(const QModelIndex &left, const QModelIndex &right) const
{
    QVariant leftData = left.data(static_cast<int>(BaseModel::BaseRole::Category));
    QString leftCategory = leftData.isNull() ? QStringLiteral("UnknownCategory") : leftData.toString();

    QVariant rightData = right.data(static_cast<int>(BaseModel::BaseRole::Category));
    QString rightCategory = rightData.isNull() ? QStringLiteral("UnknownCategory") : rightData.toString();

    int leftIndex = s_categoryOrder.indexOf(leftCategory);
    if (leftIndex == -1) {
        leftIndex = s_categoryOrder.indexOf(QStringLiteral("UnknownCategory"));
    }

    int rightIndex = s_categoryOrder.indexOf(rightCategory);
    if (rightIndex == -1) {
        rightIndex = s_categoryOrder.indexOf(QStringLiteral("UnknownCategory"));
    }

    return leftIndex - rightIndex;
}

#include "moc_sortedsystemtraymodel.cpp"
