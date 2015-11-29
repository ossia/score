#include <qboxlayout.h>
#include <qlabel.h>
#include <qstring.h>
#include <qtoolbutton.h>

#include "AddSlotWidget.hpp"
#include "RackInspectorSection.hpp"

AddSlotWidget::AddSlotWidget(RackInspectorSection* parent) :
    QWidget {parent}
{
    QHBoxLayout* layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0 , 0);
    this->setLayout(layout);

    // Button
    QToolButton* addButton = new QToolButton;
    addButton->setText("+");
    addButton->setObjectName("addAutom");

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
