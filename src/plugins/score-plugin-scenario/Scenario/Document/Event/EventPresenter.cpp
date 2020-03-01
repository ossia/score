// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "EventPresenter.hpp"

#include <Scenario/Commands/Event/State/AddStateWithData.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/Expression.hpp>
#include <State/Message.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/MimeVisitor.hpp>
#include <score/tools/Bind.hpp>

#include <QGraphicsItem>
#include <QMimeData>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::EventPresenter)

namespace Scenario
{
EventPresenter::EventPresenter(
    const EventModel& model,
    QGraphicsItem* parentview,
    QObject* parent)
    : QObject{parent}, m_model{model}, m_view{new EventView{*this, parentview}}
{
  // The scenario catches this :
  con(m_model.selection,
      &Selectable::changed,
      m_view,
      &EventView::setSelected);

  con(m_model.metadata(),
      &score::ModelMetadata::ColorChanged,
      m_view,
      [this] { m_view->update(); });

  con(m_model.metadata(),
      &score::ModelMetadata::CommentChanged,
      m_view,
      &EventView::changeToolTip);

  con(m_model, &EventModel::statusChanged, m_view,
      [this] {
    m_view->update();
    m_view->conditionItem().update();
  });

  connect(
      m_view,
      &EventView::eventHoverEnter,
      this,
      &EventPresenter::eventHoverEnter);

  connect(
      m_view,
      &EventView::eventHoverLeave,
      this,
      &EventPresenter::eventHoverLeave);

  connect(m_view, &EventView::dropReceived, this, &EventPresenter::handleDrop);

  m_view->setCondition(m_model.condition().toString());
  m_view->setToolTip(m_model.metadata().getComment());

  con(m_model,
      &EventModel::conditionChanged,
      this,
      [&](const State::Expression& c) { m_view->setCondition(c.toString()); });
}

EventPresenter::~EventPresenter() {}

VerticalExtent EventPresenter::extent() const noexcept
{
  return m_extent;
}

void EventPresenter::setExtent(const VerticalExtent& extent)
{
  if (extent != m_extent)
  {
    m_extent = extent;
    extentChanged(m_extent);
  }
}

void EventPresenter::addState(StatePresenter* ev)
{
  SCORE_ASSERT(!ossia::contains(m_states, ev));
  m_states.push_back(ev);
}

void EventPresenter::removeState(StatePresenter* ev)
{
  auto it = ossia::find(m_states, ev);
#if defined(SCORE_DEBUG)
  SCORE_ASSERT(it != m_states.end());
#endif
  if(it != m_states.end())
     m_states.erase(it);
}

const Id<EventModel>& EventPresenter::id() const
{
  return m_model.id();
}

EventView* EventPresenter::view() const
{
  return m_view;
}

const EventModel& EventPresenter::model() const
{
  return m_model;
}

bool EventPresenter::isSelected() const
{
  return m_model.selection.get();
}

void EventPresenter::handleDrop(const QPointF& pos, const QMimeData& mime)
{
  // We don't want to create a new state in BaseScenario
  auto scenar = dynamic_cast<Scenario::ProcessModel*>(m_model.parent());
  // todo Maybe the drop should be handled by the scenario presenter ?? or not

  // If the mime data has states in it we can handle it.
  if (scenar && mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();

    RedoMacroCommandDispatcher<Command::AddStateWithData> dispatcher{
        score::IDocument::documentContext(m_model).commandStack};

    auto cmd = new Command::CreateState{
        *scenar,
        m_model.id(),
        pos.y() / m_view->parentItem()->boundingRect().size().height()};
    dispatcher.submit(cmd);
    dispatcher.submit(new Command::AddMessagesToState{
        scenar->states.at(cmd->createdState()), std::move(ml)});

    dispatcher.commit();
  }
}
}
