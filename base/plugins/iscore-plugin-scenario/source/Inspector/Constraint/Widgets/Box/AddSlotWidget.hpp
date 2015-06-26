#pragma once
#include <QWidget>

class BoxInspectorSection;
class AddSlotWidget : public QWidget
{
        Q_OBJECT

    public:
        AddSlotWidget(BoxInspectorSection* parent);
};
