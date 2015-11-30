#include "PanelPresenter.hpp"
#include <core/presenter/Presenter.hpp>

using namespace iscore;


PanelPresenter::PanelPresenter(
        Presenter* parent_presenter,
        PanelView* view) :
    QObject {parent_presenter},
    m_view {view},
    m_parentPresenter {parent_presenter}
{

}

void PanelPresenter::setModel(PanelModel* model)
{
    m_model = model;
    on_modelChanged();
}

PanelModel*PanelPresenter::model() const
{
    return m_model;
}

PanelView*PanelPresenter::view() const
{
    return m_view;
}

Presenter*PanelPresenter::presenter() const
{
    return m_parentPresenter;
}

iscore::PanelPresenter::~PanelPresenter()
{

}
