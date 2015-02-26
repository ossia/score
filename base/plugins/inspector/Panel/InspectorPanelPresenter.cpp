#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelView.hpp"

InspectorPanelPresenter::InspectorPanelPresenter(iscore::Presenter* parent,
        iscore::PanelViewInterface* view) :
    iscore::PanelPresenterInterface {parent, view}
{

}

void InspectorPanelPresenter::on_modelChanged()
{
    auto i_model = static_cast<InspectorPanelModel*>(m_model);
    auto i_view  = static_cast<InspectorPanelView*>(m_view);

    i_view->disconnect(m_mvConnection);
    m_mvConnection = connect(
                         i_model, &InspectorPanelModel::setNewItem,
                         i_view , &InspectorPanelView::on_setNewItem);
}
