#pragma once

#include <QLineEdit>

namespace Lumen
{
/// Windows address-row search box; DolphinMainWindow routes its term to the active view's search bar.
class SearchField : public QLineEdit
{
    Q_OBJECT
public:
    explicit SearchField(QWidget *parent);

Q_SIGNALS:
    void escapePressed();
    void focusViewRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
};
}
