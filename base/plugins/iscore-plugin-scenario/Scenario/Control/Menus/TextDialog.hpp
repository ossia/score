#pragma once
#include <QDialog>

// Has a big selectable text area.
class TextDialog final : public QDialog
{
    public:
        TextDialog(const QString& s, QWidget* parent);
};
