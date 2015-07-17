#include "StatePresenter.hpp"
#include "DisplayedStateModel.hpp"
#include "StateView.hpp"

#include "Commands/Event/AddStateToEvent.hpp"
#include "Commands/Event/State/AddStateWithData.hpp"
#include <State/StateMimeTypes.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <QGraphicsScene>
#include <QMimeData>
#include <QJsonDocument>

StatePresenter::StatePresenter(
        const StateModel &model,
        QGraphicsItem *parentview,
        QObject *parent) :
    NamedObject {"StatePresenter", parent},
    m_model {model},
    m_view {new StateView{*this, parentview}},
    m_dispatcher{iscore::IDocument::documentFromObject(m_model)->commandStack()}
{
    // The scenario catches this :
    connect(&m_model.selection, &Selectable::changed,
            m_view, &StateView::setSelected);

    connect(&(m_model.metadata),  &ModelMetadata::colorChanged,
            m_view,               &StateView::changeColor);

    connect(&m_model, &StateModel::stateAdded, this, &StatePresenter::updateStateView);
    connect(&m_model, &StateModel::stateRemoved, this, &StatePresenter::updateStateView);
    connect(&m_model, &StateModel::statesReplaced, this, &StatePresenter::updateStateView);

    connect(m_view, &StateView::dropReceived,
            this, &StatePresenter::handleDrop);
}

StatePresenter::~StatePresenter()
{
    // TODO we really need to refactor this.
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

const id_type<StateModel> &StatePresenter::id() const
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
    if(mime->formats().contains(iscore::mime::state()))
    {
        Deserializer<JSONObject> deser{
            QJsonDocument::fromJson(mime->data(iscore::mime::state())).object()};
        iscore::State s;
        deser.writeTo(s);

        auto cmd = new Scenario::Command::AddStateToStateModel{
                iscore::IDocument::path(m_model),
                std::move(s)};
        m_dispatcher.submitCommand(cmd);
    }
}

void StatePresenter::updateStateView()
{
    m_view->setContainMessage(!m_model.states().empty());
}

