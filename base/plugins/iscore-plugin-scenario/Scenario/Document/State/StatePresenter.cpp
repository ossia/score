#include <QMimeData>
#include <QStringList>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <State/MessageListSerialization.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

#include "StateModel.hpp"
#include "StatePresenter.hpp"
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <State/Message.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/serialization/MimeVisitor.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <QApplication>

#include <iscore/tools/Todo.hpp>

class QObject;
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
StatePresenter::StatePresenter(
    const StateModel& model, QQuickPaintedItem* parentview, QObject* parent)
    : QObject{parent}
    , m_model{model}
    , m_view{new StateView{*this, parentview}}
    , m_dispatcher{iscore::IDocument::documentContext(m_model).commandStack}
{
  // The scenario catches this :
  con(m_model.selection, &Selectable::changed, m_view,
      &StateView::setSelected);

  con(m_model.metadata(), &iscore::ModelMetadata::ColorChanged, m_view,
      &StateView::changeColor);

  con(m_model, &StateModel::sig_statesUpdated, this,
      &StatePresenter::updateStateView);

  con(m_model, &StateModel::statusChanged, m_view, &StateView::setStatus);
  m_view->setStatus(m_model.status());


  connect(m_view, &StateView::startCreateMode, this, [=] {
    auto& plug = iscore::AppContext().applicationPlugin<Scenario::ScenarioApplicationPlugin>();
    plug.editionSettings().setTool(Scenario::Tool::Create);
  });
  connect(m_view, &StateView::dropReceived, this, &StatePresenter::handleDrop);

  updateStateView();
}

StatePresenter::~StatePresenter()
{
}

const Id<StateModel>& StatePresenter::id() const
{
  return m_model.id();
}

StateView* StatePresenter::view() const
{
  return m_view;
}

const StateModel& StatePresenter::model() const
{
  return m_model;
}

bool StatePresenter::isSelected() const
{
  return m_model.selection.get();
}

void StatePresenter::handleDrop(const QMimeData* mime)
{
  // If the mime data has states in it we can handle it.
  if (mime->formats().contains(iscore::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{*mime};
    State::MessageList ml = des.deserialize();

    auto cmd = new Command::AddMessagesToState{m_model, ml};

    m_dispatcher.submitCommand(cmd);
  }
}

void StatePresenter::updateStateView()
{
  m_view->setContainMessage(m_model.messages().rootNode().hasChildren());
}
}
