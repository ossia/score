#pragma once
#include <QWidget>

namespace Scenario
{
class RackInspectorSection;

class AddSlotWidget final : public QWidget
{
    public:
        AddSlotWidget(RackInspectorSection* parent);
};
}
