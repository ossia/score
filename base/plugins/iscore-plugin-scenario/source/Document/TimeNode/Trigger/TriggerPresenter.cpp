#include "TriggerPresenter.hpp"
#include "TriggerModel.hpp"
#include "TriggerView.hpp"

TriggerPresenter::TriggerPresenter(TriggerModel* model, QGraphicsObject* parentView, QObject* parent):
    m_model{model},
    m_view{new TriggerView{parentView} },
    QObject{parent}
{

}

