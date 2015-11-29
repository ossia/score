#include <State/MessageListSerialization.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
#include <qmimedata.h>
#include <qstringlist.h>

#include "Process/ModelMetadata.hpp"
#include "Scenario/Commands/State/AddMessagesToState.hpp"
#include "Scenario/Document/Event/ExecutionStatus.hpp"
#include "Scenario/Document/State/ItemModel/MessageItemModel.hpp"
#include "State/Message.hpp"
#include "StateModel.hpp"
#include "StatePresenter.hpp"
#include "StateView.hpp"
#include "iscore/command/Dispatchers/CommandDispatcher.hpp"
#include "iscore/selection/Selectable.hpp"
#include "iscore/serialization/MimeVisitor.hpp"
#include "iscore/tools/IdentifiedObject.hpp"
#include "iscore/tools/NamedObject.hpp"
#include "iscore/tools/Todo.hpp"

class QObject;
template <typename tag, typename impl> class id_base_t;

StatePresenter::StatePresenter(
        const StateModel &model,
        QGraphicsItem *parentview,
        QObject *parent) :
    NamedObject {"StatePresenter", parent},
    m_model {model},
    m_view {new StateView{*this, parentview}},
    m_dispatcher{iscore::IDocument::commandStack(m_model)}
{
    // The scenario catches this :
    con(m_model.selection, &Selectable::changed,
            m_view, &StateView::setSelected);

    con(m_model.metadata,  &ModelMetadata::colorChanged,
        m_view, &StateView::changeColor);

    con(m_model, &StateModel::sig_statesUpdated,
            this, &StatePresenter::updateStateView);

    con(m_model, &StateModel::statusChanged,
        this, [&] (ExecutionStatus e)
    {
        m_view->changeColor(eventStatusColor(e));
    });

    connect(m_view, &StateView::dropReceived,
            this, &StatePresenter::handleDrop);

    updateStateView();
    m_view->changeColor(eventStatusColor(m_model.status()));
}

StatePresenter::~StatePresenter()
{
    deleteGraphicsObject(m_view);
}

const Id<StateModel> &StatePresenter::id() const
{
    return m_model.id();
}

StateView *StatePresenter::view() const
{
    return m_view;
}

const StateModel &StatePresenter::model() const
{
    return m_model;
}

bool StatePresenter::isSelected() const
{
    return m_model.selection.get();
}

void StatePresenter::handleDrop(const QMimeData *mime)
{
    // If the mime data has states in it we can handle it.
    if(mime->formats().contains(iscore::mime::messagelist()))
    {
        Mime<iscore::MessageList>::Deserializer des{*mime};
        iscore::MessageList ml = des.deserialize();

        auto cmd = new AddMessagesToState{
                   iscore::IDocument::path(m_model.messages()),
                   ml};

        m_dispatcher.submitCommand(cmd);
    }
}

void StatePresenter::updateStateView()
{
    m_view->setContainMessage(m_model.messages().rootNode().hasChildren());
}

