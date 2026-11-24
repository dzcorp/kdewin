#include "lumentabhost.h"

#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

LumenTabHost::LumenTabHost(QWidget *parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);
}

void LumenTabHost::setTabBar(QTabBar *bar)
{
    m_bar = bar;
    connect(bar, &QTabBar::currentChanged, this, [this](int index) {
        m_stack->setCurrentIndex(index);
        Q_EMIT currentChanged(index);
    });
    connect(bar, &QTabBar::tabCloseRequested, this, &LumenTabHost::tabCloseRequested);
    connect(bar, &QTabBar::tabMoved, this, [this](int from, int to) {
        const QSignalBlocker block(m_stack);
        m_stack->insertWidget(to, m_stack->widget(from));
        m_stack->setCurrentIndex(m_bar->currentIndex());
    });
}

QTabBar *LumenTabHost::tabBar() const
{
    return m_bar;
}

int LumenTabHost::count() const
{
    return m_stack->count();
}

int LumenTabHost::currentIndex() const
{
    return m_bar->currentIndex();
}

QWidget *LumenTabHost::widget(int index) const
{
    return m_stack->widget(index);
}

int LumenTabHost::indexOf(const QWidget *page) const
{
    return m_stack->indexOf(page);
}

int LumenTabHost::insertTab(int index, QWidget *page, const QIcon &icon, const QString &label)
{
    index = m_stack->insertWidget(index, page);
    m_bar->insertTab(index, icon, label);
    m_stack->setCurrentIndex(m_bar->currentIndex());
    tabInserted(index);
    return index;
}

void LumenTabHost::removeTab(int index)
{
    QWidget *page = m_stack->widget(index);
    if (!page) {
        return;
    }
    m_stack->removeWidget(page);
    m_bar->removeTab(index);
    m_stack->setCurrentIndex(m_bar->currentIndex());
    tabRemoved(index);
}

void LumenTabHost::setCurrentIndex(int index)
{
    m_bar->setCurrentIndex(index);
}

void LumenTabHost::setElideMode(Qt::TextElideMode mode)
{
    m_bar->setElideMode(mode);
}

void LumenTabHost::setUsesScrollButtons(bool on)
{
    m_bar->setUsesScrollButtons(on);
}
