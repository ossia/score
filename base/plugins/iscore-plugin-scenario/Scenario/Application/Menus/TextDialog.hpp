#pragma once

#include <qdialog.h>
#include <qstring.h>

class QWidget;

// Has a big selectable text area.
class TextDialog final : public QDialog
{
    public:
        TextDialog(const QString& s, QWidget* parent);
};
