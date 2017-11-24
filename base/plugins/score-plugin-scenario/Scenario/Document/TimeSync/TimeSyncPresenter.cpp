// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/TimeSync/TriggerView.hpp>
#include <score/widgets/GraphicsItem.hpp>

#include "TimeSyncPresenter.hpp"
#include <score/model/ModelMetadata.hpp>
#include <score/selection/Selectable.hpp>
#include <State/MessageListSerialization.hpp>
#include <Scenario/Commands/TimeSync/SetTrigger.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <score/tools/Todo.hpp>

class QObject;
#include <score/model/Identifier.hpp>

namespace Scenario
{
TimeSyncPresenter::TimeSyncPresenter(
    const TimeSyncModel& model,
        QGraphicsItem* parentview,
        QObject* parent)
    : QObject{parent}
    , m_model{model}
    , m_view{new TimeSyncView{*this, parentview}}
    , m_triggerView{new TriggerView{m_view}}
{
  con(m_model.selection, &Selectable::changed, this,
      [=](bool b) { m_view->setSelected(b); });

  con(m_model, &TimeSyncModel::newEvent, this,
      &TimeSyncPresenter::on_eventAdded);

  con(m_model.metadata(), &score::ModelMetadata::ColorChanged, this,
      [=](const score::ColorRef& c) { m_view->changeColor(c); });
  con(m_model.metadata(), &score::ModelMetadata::LabelChanged, this,
      [=](const auto& t) { m_view->setLabel(t); });
  con(m_model, &TimeSyncModel::activeChanged, this, [=] {
    m_view->setTriggerActive(m_model.active());
    m_triggerView->setVisible(m_model.active());
    m_triggerView->setToolTip(m_model.expression().toString());
  });

  m_view->changeColor(m_model.metadata().getColor());
  m_view->setLabel(m_model.metadata().getLabel());
  m_view->setTriggerActive(m_model.active());
  // TODO find a correct way to handle validity of model elements.
  // extentChanged is updated in scenario.

  m_triggerView->setVisible(m_model.active());
  m_triggerView->setPos(-7.5, -25.);

  m_triggerView->setToolTip(m_model.expression().toString());
  con(m_model, &TimeSyncModel::triggerChanged, this,
      [&](const State::Expression& t) { m_triggerView->setToolTip(t.toString()); });

  connect(
      m_triggerView, &TriggerView::pressed,
              &m_model, [=] (QPointF sp) {
      m_model.triggeredByGui();
      pressed(sp);
  });

  connect(m_triggerView, &TriggerView::dropReceived,
          this, &TimeSyncPresenter::handleDrop);

}

TimeSyncPresenter::~TimeSyncPresenter()
{
}

const Id<TimeSyncModel>& TimeSyncPresenter::id() const
{
  return m_model.id();
}

const TimeSyncModel& TimeSyncPresenter::model() const
{
  return m_model;
}

TimeSyncView* TimeSyncPresenter::view() const
{
  return m_view;
}

void TimeSyncPresenter::on_eventAdded(const Id<EventModel>& eventId)
{
  emit eventAdded(eventId, m_model.id());
}

void TimeSyncPresenter::handleDrop(const QPointF& pos, const QMimeData* mime)
{
  // We don't want to create a Trigger in BaseScenario
  auto scenar = dynamic_cast<Scenario::TimeSyncModel*>(m_model.parent());
  // todo Maybe the drop should be handled by the scenario presenter ?? or not

  // If the mime data has states in it we can handle it.
  if (scenar && mime->formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{*mime};
    State::MessageList ml = des.deserialize();

    if (ml.size() > 0)
    {
      auto trig = State::parseExpression(ml[0].address.toString());

      CommandDispatcher dispatcher{
        score::IDocument::documentContext(m_model).commandStack};
      auto cmd = new Command::SetTrigger{m_model, std::move(*trig)};
      dispatcher.submitCommand(cmd);
    }
  }
}
}
