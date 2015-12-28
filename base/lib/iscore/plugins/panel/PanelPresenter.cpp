#include "PanelPresenter.hpp"
#include <core/presenter/Presenter.hpp>

using namespace iscore;


PanelPresenter::PanelPresenter(
        PanelView* view,
        QObject* parent) :
    QObject {parent},
    m_view {view}
{

}

void PanelPresenter::setModel(PanelModel* model)
{
    auto old = m_model;
    m_model = model;
    on_modelChanged(old, model);
}

PanelModel*PanelPresenter::model() const
{
    return m_model;
}

PanelView*PanelPresenter::view() const
{
    return m_view;
}

iscore::PanelPresenter::~PanelPresenter()
{

}
