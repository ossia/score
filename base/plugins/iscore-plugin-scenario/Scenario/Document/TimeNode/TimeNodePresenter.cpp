#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerPresenter.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

#include "TimeNodePresenter.hpp"
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/selection/Selectable.hpp>

#include <iscore/tools/Todo.hpp>

class QObject;
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
TimeNodePresenter::TimeNodePresenter(
    const TimeNodeModel& model, QQuickPaintedItem* parentview, QObject* parent)
    : QObject{parent}
    , m_model{model}
    , m_view{new TimeNodeView{*this, parentview}}
{
  m_triggerPres = new TriggerPresenter{*m_model.trigger(), m_view, this};

  con(m_model.selection, &Selectable::changed, this,
      [=](bool b) { m_view->setSelected(b); });

  con(m_model, &TimeNodeModel::newEvent, this,
      &TimeNodePresenter::on_eventAdded);

  con(m_model.metadata(), &iscore::ModelMetadata::ColorChanged, this,
      [=](const iscore::ColorRef& c) { m_view->changeColor(c); });
  con(m_model.metadata(), &iscore::ModelMetadata::LabelChanged, this,
      [=](const auto& t) { m_view->setLabel(t); });
  connect(m_model.trigger(), &TriggerModel::activeChanged, this, [=] {
    m_view->setTriggerActive(m_model.trigger()->active());
  });
  m_view->changeColor(m_model.metadata().getColor());
  m_view->setLabel(m_model.metadata().getLabel());
  m_view->setTriggerActive(m_model.trigger()->active());
  // TODO find a correct way to handle validity of model elements.
  // extentChanged is updated in scenario.
}

TimeNodePresenter::~TimeNodePresenter()
{
}

const Id<TimeNodeModel>& TimeNodePresenter::id() const
{
  return m_model.id();
}

const TimeNodeModel& TimeNodePresenter::model() const
{
  return m_model;
}

TimeNodeView* TimeNodePresenter::view() const
{
  return m_view;
}

void TimeNodePresenter::on_eventAdded(const Id<EventModel>& eventId)
{
  emit eventAdded(eventId, m_model.id());
}
}
