// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "EventSummaryWidget.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <State/Expression.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QGridLayout>

namespace Scenario
{
EventSummaryWidget::EventSummaryWidget(
    const EventModel& object,
    const score::DocumentContext& doc,
    QWidget* parent)
    : QWidget{parent}
    , event{object}
    , m_selectionDispatcher{new score::SelectionDispatcher{doc.selectionStack}}
{
  auto mainLay = new score::MarginLess<QGridLayout>{this};

  auto eventBtn = SelectionButton::make("", &object, *m_selectionDispatcher, this);

  mainLay->addWidget(new TextLabel{object.metadata().getName()}, 0, 0, 1, 3);
  mainLay->addWidget(new TextLabel{object.date().toString()}, 0, 3, 1, 3);
  mainLay->addWidget(eventBtn, 0, 6, 1, 1);

  if (!object.condition().toString().isEmpty())
  {
    auto cond = new TextLabel{object.condition().toString()};
    cond->setWordWrap(true);
    mainLay->addWidget(cond, 1, 1, 1, 6);
  }
}

EventSummaryWidget::~EventSummaryWidget() { }
}
