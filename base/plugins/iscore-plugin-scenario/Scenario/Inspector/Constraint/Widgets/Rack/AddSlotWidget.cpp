#include <QBoxLayout>
#include <QLabel>
#include <QString>
#include <QToolButton>

#include "AddSlotWidget.hpp"
#include "RackInspectorSection.hpp"
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SetIcons.hpp>

#include <iscore/widgets/TextLabel.hpp>

namespace Scenario
{
AddSlotWidget::AddSlotWidget(RackInspectorSection* parent) : QWidget{parent}
{
  auto layout = new iscore::MarginLess<QHBoxLayout>(this);

  // Button
  auto addButton = new QToolButton;
  addButton->setText(QStringLiteral("+"));
  addButton->setObjectName("addAutom");
  auto addIcon = makeIcons(
      ":/icons/condition_add_on.png", ":/icons/condition_add_off.png");
  addButton->setIcon(addIcon);

  // Text
  auto text = new TextLabel(tr("Add Slot"));
  text->setStyleSheet(QStringLiteral("text-align : left;"));

  layout->addWidget(addButton);
  layout->addWidget(text);

  connect(addButton, &QToolButton::pressed, [=]() { parent->createSlot(); });
}
}
