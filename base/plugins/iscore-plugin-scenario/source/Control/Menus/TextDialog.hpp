#pragma once
#include <QDialog>

// Has a big selectable text area.
class TextDialog : public QDialog
{
    public:
        TextDialog(const QString& s, QWidget* parent);
};
