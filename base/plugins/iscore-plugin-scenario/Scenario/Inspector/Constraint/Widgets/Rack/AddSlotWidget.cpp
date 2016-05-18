#include <QBoxLayout>
#include <QLabel>
#include <QString>
#include <QToolButton>

#include "AddSlotWidget.hpp"
#include "RackInspectorSection.hpp"
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SetIcons.hpp>

namespace Scenario
{
AddSlotWidget::AddSlotWidget(RackInspectorSection* parent) :
    QWidget {parent}
{
    QHBoxLayout* layout = new iscore::MarginLess<QHBoxLayout>(this);

    // Button
    QToolButton* addButton = new QToolButton;
    addButton->setText("+");
    addButton->setObjectName("addAutom");
    QIcon addIcon;
    makeIcons(&addIcon, QString(":/icons/condition_add_on.png"), QString(":/icons/condition_add_off.png"));
    addButton->setIcon(addIcon);

    // Text
    auto text = new QLabel("Add Slot");
    text->setStyleSheet(QString("text-align : left;"));

    layout->addWidget(addButton);
    layout->addWidget(text);

    connect(addButton, &QToolButton::pressed,
    [ = ]()
    {
        parent->createSlot();
    });
}
}
