#pragma once
#include <QWidget>

class RackInspectorSection;
class AddSlotWidget : public QWidget
{
        Q_OBJECT

    public:
        AddSlotWidget(RackInspectorSection* parent);
};
