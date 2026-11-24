#pragma once

#include <QIcon>
#include <QWidget>

class QStackedWidget;
class QTabBar;

/// The QTabWidget API Dolphin uses, with the tab bar living elsewhere (the Lumen title row).
class LumenTabHost : public QWidget
{
    Q_OBJECT
public:
    explicit LumenTabHost(QWidget *parent);

    void setTabBar(QTabBar *bar);
    QTabBar *tabBar() const;

    int count() const;
    int currentIndex() const;
    QWidget *widget(int index) const;
    int indexOf(const QWidget *page) const;
    int insertTab(int index, QWidget *page, const QIcon &icon, const QString &label);
    void removeTab(int index);

    void setDocumentMode(bool) {}
    void setElideMode(Qt::TextElideMode mode);
    void setUsesScrollButtons(bool on);
    /// The title row always shows its tabs.
    void setTabBarAutoHide(bool) {}

public Q_SLOTS:
    void setCurrentIndex(int index);

Q_SIGNALS:
    void currentChanged(int index);
    void tabCloseRequested(int index);

protected:
    virtual void tabInserted(int) {}
    virtual void tabRemoved(int) {}

private:
    QStackedWidget *m_stack;
    QTabBar *m_bar = nullptr;
};
