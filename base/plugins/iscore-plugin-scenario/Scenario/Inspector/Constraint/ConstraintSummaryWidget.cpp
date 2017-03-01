#include "ConstraintSummaryWidget.hpp"

#include <iscore/document/DocumentContext.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/TextLabel.hpp>

#include <QGridLayout>
#include <QLabel>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/Process.hpp>

namespace Scenario
{
ConstraintSummaryWidget::ConstraintSummaryWidget(
    const ConstraintModel& object,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : QWidget(parent)
    , m_selectionDispatcher{
          new iscore::SelectionDispatcher{doc.selectionStack}}
{
  auto mainLay = new iscore::MarginLess<QGridLayout>{this};

  auto eventBtn
      = SelectionButton::make("", &object, *m_selectionDispatcher, this);

  auto l1 = new TextLabel{object.metadata().getName()};
  auto l2 = new TextLabel{tr("start : ") + object.startDate().toString()};
  mainLay->addWidget(l1, 0, 0, 1, 3);
  mainLay->addWidget(l2, 0, 3, 1, 3);
  mainLay->addWidget(eventBtn, 0, 6, 1, 1);

  if (object.duration.isRigid())
  {
    auto l3 = new TextLabel{object.duration.defaultDuration().toString()};
    mainLay->addWidget(l3, 1, 1, 1, 4);
  }
  else
  {
    QString text = tr("Flexible : ")
                   % (object.duration.minDuration().isZero()
                        ? QStringLiteral("0")
                        : object.duration.minDuration().toString())
                   % tr(" to ")
                   % (object.duration.maxDuration().isInfinite()
                                                 ? QStringLiteral("inf")
                                                 : object.duration.maxDuration().toString());
    auto l4 = new TextLabel{text};
    mainLay->addWidget(l4, 1, 1, 1, 4);
  }

  if (!object.processes.empty())
  {
    auto processList
        = new Inspector::InspectorSectionWidget{tr("Processes"), false, this};
    for (const auto& p : object.processes)
    {
      auto lab = new TextLabel{p.prettyName()};
      processList->addContent(lab);
    }
    mainLay->addWidget(processList, 2, 1, 1, 6);
  }
}
}
