#include "lumensearchfield.h"

#include "lumenglyph.h"
#include "lumentokens.h"

#include <KLocalizedString>

#include <QAction>
#include <QKeyEvent>

namespace Lumen
{
SearchField::SearchField(QWidget *parent)
    : QLineEdit(parent)
{
    setPlaceholderText(i18nc("@action:intoolbar", "Search"));
    setClearButtonEnabled(true);
    // Explorer's search and address boxes use Body text (14 px), not the 12 px UI font.
    QFont body = font();
    body.setPixelSize(int(T::Type::Body[0]));
    setFont(body);
    setFixedWidth(fontMetrics().height() * 14);
    QAction *search = addAction(glyphIcon(Glyph::Search), QLineEdit::TrailingPosition);
    connect(search, &QAction::triggered, this, &QLineEdit::returnPressed);
}

void SearchField::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        Q_EMIT escapePressed();
        return;
    }
    if (event->key() == Qt::Key_Down) {
        Q_EMIT focusViewRequested();
        return;
    }
    QLineEdit::keyPressEvent(event);
}
}
