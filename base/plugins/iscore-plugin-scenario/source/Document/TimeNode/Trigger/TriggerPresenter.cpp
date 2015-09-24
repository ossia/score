#include "TriggerPresenter.hpp"
#include "TriggerModel.hpp"
#include "TriggerView.hpp"

TriggerPresenter::TriggerPresenter(TriggerModel* model, QGraphicsObject* parentView, QObject* parent):
    m_model{model},
    m_view{new TriggerView{parentView} },
    QObject{parent}
{
    m_view->setVisible(model->active());
    connect(m_model, &TriggerModel::activeChanged,
            this, [&] ()
    {
        m_view->setVisible(m_model->active());
    });
}

