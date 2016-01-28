#pragma once
#include <QPlainTextEdit>

// TODO put somewhere in i-score base, it's an useful
// generic component.
class NotifyingPlainTextEdit final : public QPlainTextEdit
{
    Q_OBJECT
    public:
    NotifyingPlainTextEdit(QString txt);

    signals:
    void editingFinished(QString newTxt);

    protected:
    void focusOutEvent(QFocusEvent* event) override;
};

