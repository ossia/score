#pragma once

#include <QPlainTextEdit>

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

