#include "NotifyingPlainTextEdit.hpp"

NotifyingPlainTextEdit::NotifyingPlainTextEdit(QString txt) :
    QPlainTextEdit{txt}
{

}

void NotifyingPlainTextEdit::focusOutEvent(QFocusEvent* event)
{
    emit editingFinished(this->toPlainText());
    event->ignore();
}

