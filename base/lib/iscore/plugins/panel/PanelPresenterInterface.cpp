#include "PanelPresenterInterface.hpp"
using namespace iscore;


PanelPresenterInterface::PanelPresenterInterface(
        Presenter* parent_presenter,
        PanelViewInterface* view) :
    QObject {parent_presenter},
    m_view {view},
    m_parentPresenter {parent_presenter}
{

}

void PanelPresenterInterface::setModel(PanelModelInterface* model)
{
    m_model = model;
    on_modelChanged();
}

PanelModelInterface*PanelPresenterInterface::model() const
{
    return m_model;
}

PanelViewInterface*PanelPresenterInterface::view() const
{
    return m_view;
}

Presenter*PanelPresenterInterface::presenter() const
{
    return m_parentPresenter;
}
