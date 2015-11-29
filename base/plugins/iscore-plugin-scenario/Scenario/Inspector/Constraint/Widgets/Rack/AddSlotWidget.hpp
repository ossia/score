#pragma once
#include <qwidget.h>

class RackInspectorSection;

class AddSlotWidget final : public QWidget
{
        Q_OBJECT

    public:
        AddSlotWidget(RackInspectorSection* parent);
};
