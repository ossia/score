#include "StatePresenter.hpp"
#include "DisplayedStateModel.hpp"
#include "StateView.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

StatePresenter::StatePresenter(
        const DisplayedStateModel &model,
        QGraphicsObject *parentview,
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

    connect(&m_model, &DisplayedStateModel::statesChanged,
            this, [&] () {
        m_view->setContainMessage(!m_model.states().empty());
    });
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

const id_type<DisplayedStateModel> &StatePresenter::id() const
{
    return m_model.id();
}

StateView *StatePresenter::view() const
{
    return m_view;
}

const DisplayedStateModel &StatePresenter::model() const
{
    return m_model;
}

bool StatePresenter::isSelected() const
{
    return m_model.selection.get();
}

void StatePresenter::handleDrop(const QMimeData *mime)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    // Use the one from EventPresenter.

}
