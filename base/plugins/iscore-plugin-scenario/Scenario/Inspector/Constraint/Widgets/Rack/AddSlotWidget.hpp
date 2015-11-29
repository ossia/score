#pragma once
#include <QWidget>

class RackInspectorSection;

class AddSlotWidget final : public QWidget
{
        Q_OBJECT

    public:
        AddSlotWidget(RackInspectorSection* parent);
};
