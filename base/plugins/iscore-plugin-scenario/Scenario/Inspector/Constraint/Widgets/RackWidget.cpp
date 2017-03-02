#include <QBoxLayout>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QObject>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Inspector/Constraint/Widgets/ProcessViewTabWidget.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/widgets/TextLabel.hpp>

#include <QString>
#include <QStringList>
#include <QToolButton>

#include "RackWidget.hpp"
#include <iscore/model/Identifier.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SetIcons.hpp>
#include <iscore/widgets/SignalUtils.hpp>

namespace Scenario
{
const QString RackWidget::hiddenText{QObject::tr("None")};

RackWidget::RackWidget(ProcessViewTabWidget* parentTabWidget, QWidget* parent)
    : QWidget{parent}
    , m_model{parentTabWidget->parentConstraint().model()}
    , m_parent{parentTabWidget}
{
  auto mainLay = new QVBoxLayout{this};
  auto mainWidg = new QWidget;
  mainLay->addWidget(mainWidg);

  auto lay = new iscore::MarginLess<QHBoxLayout>{mainWidg};

  // Button
  auto addButton = new QToolButton{this};
  addButton->setText(QStringLiteral("+"));
  auto addIcon = makeIcons(
      ":/icons/condition_add_on.png", ":/icons/condition_add_off.png");
  addButton->setIcon(addIcon);

  connect(addButton, &QToolButton::pressed, [=]() {
    parentTabWidget->createRack();
  });
  // Text
  auto addText = new TextLabel{tr("Add Rack"), this};
  addText->setStyleSheet(QStringLiteral("text-align : left;"));

  // For each view model, a rack chooser.
  viewModelsChanged();

  // Layout setup
  lay->addWidget(addButton);
  lay->addWidget(addText);

  mainLay->addStretch(1);
}

RackWidget::~RackWidget() = default;

void RackWidget::viewModelsChanged()
{
  delete m_comboBoxesWidget;
  m_comboBoxesWidget = new QWidget{this};
  auto lay = new iscore::MarginLess<QGridLayout>{m_comboBoxesWidget};
  int i = 0;

  auto rackUsed = new TextLabel{tr("Rack Used")};
  rackUsed->setAlignment(Qt::AlignHCenter);

  lay->addWidget(new QWidget{m_comboBoxesWidget}, i, 0);
  auto lab = new TextLabel{tr("View Model")};
  lay->addWidget(lab, i, 1);
  lay->addWidget(rackUsed, i, 2);
  lay->addWidget(new QWidget{m_comboBoxesWidget}, i, 3);
  i++;

  for (auto vm : m_model.viewModels())
  {
    QLabel* label;
    if (dynamic_cast<FullViewConstraintViewModel*>(vm))
    {
      label = new TextLabel{tr("Full view"), m_comboBoxesWidget};
    }
    else
    {
      //            label = new TextLabel{QString::number(vm->id().val().get()),
      //            m_comboBoxesWidget};
      // TODO until we have others viewmodel, display a name instead of Id
      label = new TextLabel{tr("Reduce view"), m_comboBoxesWidget};
    }
    auto rack = new QComboBox{m_comboBoxesWidget};
    updateComboBox(rack, vm);

    lay->addWidget(new QWidget{m_comboBoxesWidget}, i, 0);
    lay->addWidget(label, i, 1);
    lay->addWidget(rack, i, 2);
    lay->addWidget(new QWidget{m_comboBoxesWidget}, i, 3);
    i++;
  }

  this->layout()->addWidget(m_comboBoxesWidget);
}

void RackWidget::updateComboBox(QComboBox* combobox, ConstraintViewModel* vm)
{
  combobox->clear();
  combobox->addItem(hiddenText);

  for (const RackModel& rack : m_model.racks)
  {
    combobox->addItem(
        rack.metadata().getName(), QVariant::fromValue(rack.id()));
    if (vm->shownRack() == rack.id())
    {
      combobox->setCurrentIndex(combobox->count() - 1);
    }
  }

  connect(
      combobox, SignalUtils::QComboBox_activated_int(), combobox,
      [ =, cvm = vm ](int i) {
        auto data = combobox->itemData(i);
        if (data.canConvert<Id<RackModel>>())
          m_parent->activeRackChanged(data.value<Id<RackModel>>(), cvm);
        else
          m_parent->activeRackChanged({}, cvm);
      });

  connect(vm, &ConstraintViewModel::rackHidden, combobox, [=]() {
    combobox->setCurrentIndex(0);
  });
  connect(vm, &ConstraintViewModel::lastRackRemoved, combobox, [=]() {
    combobox->setCurrentIndex(0);
  });

  connect(
      vm, &ConstraintViewModel::rackShown, combobox,
      [=](OptionalId<RackModel> id) {
        using namespace std;
        if (id)
        {
          int n = combobox->findData(QVariant::fromValue(*id));
          if (n != -1)
            combobox->setCurrentIndex(n);
        }
      });
}
}
