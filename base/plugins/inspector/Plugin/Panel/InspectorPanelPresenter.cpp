#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelView.hpp"
#include "InspectorPanelId.hpp"

#include <core/document/DocumentPresenter.hpp>
InspectorPanelPresenter::InspectorPanelPresenter(iscore::Presenter* parent,
        iscore::PanelViewInterface* view) :
    iscore::PanelPresenterInterface {parent, view}
{

}

int InspectorPanelPresenter::panelId() const
{
    return INSPECTOR_PANEL_ID;
}

void InspectorPanelPresenter::on_modelChanged()
{
    using namespace iscore;
    if(model())
    {
        auto doc = IDocument::documentFromObject(model());
        auto panelview = static_cast<InspectorPanelView *>(view());
        auto panelmodel = static_cast<InspectorPanelModel *>(model());
        panelview->setCurrentDocument(doc);

        disconnect(m_mvConnection);
        m_mvConnection = connect(panelmodel, &InspectorPanelModel::selectionChanged,
                                 panelview, &InspectorPanelView::setNewSelection);
    }
    else
    {
        auto panelview = static_cast<InspectorPanelView *>(view());
        panelview->setCurrentDocument(nullptr);

        disconnect(m_mvConnection);
    }
}
