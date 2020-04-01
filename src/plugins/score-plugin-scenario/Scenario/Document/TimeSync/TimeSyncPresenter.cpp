// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncPresenter.hpp"

#include <Scenario/Commands/TimeSync/SetTrigger.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/TimeSync/TriggerView.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/Bind.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TimeSyncPresenter)
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
  con(m_model.selection, &Selectable::changed, this, [=](bool b) {
    m_view->setSelected(b);
    m_triggerView->setSelected(b);
  });

  con(m_model.metadata(),
      &score::ModelMetadata::ColorChanged,
      this,
      [=](const score::ColorRef& c) { m_view->changeColor(c.getBrush()); });
  con(m_model.metadata(),
      &score::ModelMetadata::LabelChanged,
      this,
      [=](const auto& t) { m_view->setLabel(t); });
  con(m_model, &TimeSyncModel::activeChanged, this, [=] {
    m_view->setTriggerActive(m_model.active());
    m_triggerView->setVisible(m_model.active());
    m_triggerView->setToolTip(m_model.expression().toString());
  });

  m_view->changeColor(m_model.metadata().getColor().getBrush());
  m_view->setLabel(m_model.metadata().getLabel());
  m_view->setTriggerActive(m_model.active());
  // TODO find a correct way to handle validity of model elements.
  // extentChanged is updated in scenario.

  m_triggerView->setVisible(m_model.active());
  m_triggerView->setPos(-10., -25.);

  m_triggerView->setToolTip(m_model.expression().toString());
  con(m_model,
      &TimeSyncModel::triggerChanged,
      this,
      [&](const State::Expression& t) {
        m_triggerView->setToolTip(t.toString());
      });

  connect(m_triggerView, &TriggerView::pressed, &m_model, [=](QPointF sp) {
    m_model.triggeredByGui();
    pressed(sp);
  });

  connect(
      m_triggerView,
      &TriggerView::dropReceived,
      this,
      &TimeSyncPresenter::handleDrop);
}

TimeSyncPresenter::~TimeSyncPresenter() {}


const VerticalExtent& TimeSyncPresenter::extent() const noexcept
{
  return m_extent;
}

void TimeSyncPresenter::setExtent(const VerticalExtent& extent)
{
  if (extent != m_extent)
  {
    m_extent = extent;
    extentChanged(m_extent);
  }
}

void TimeSyncPresenter::addEvent(EventPresenter* ev)
{
  m_events.push_back(ev);
}

void TimeSyncPresenter::removeEvent(EventPresenter* ev)
{
  auto it = ossia::find(m_events, ev);
  if(it != m_events.end())
     m_events.erase(it);
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

TriggerView& TimeSyncPresenter::trigger() const noexcept
{
  return *m_triggerView;
}

void TimeSyncPresenter::handleDrop(const QPointF& pos, const QMimeData& mime)
{
  // If the mime data has states in it we can handle it.
  if (mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();

    if (ml.size() > 0)
    {
      QString expr = "{" + toString(State::Pulse{ml[0].address.address}) + "}";
      auto trig = State::parseExpression(expr);

      if (trig)
      {
        CommandDispatcher<> dispatcher{
            score::IDocument::documentContext(m_model).commandStack};
        auto cmd = new Command::SetTrigger{m_model, std::move(*trig)};
        dispatcher.submit(cmd);
      }
    }
  }
}
}
