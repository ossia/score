#include "InspectorPanelModel.hpp"
#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelView.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include "InspectorPanelId.hpp"

namespace iscore {
class PanelView;

}  // namespace iscore

InspectorPanelPresenter::InspectorPanelPresenter(
        iscore::PanelView* view,
        QObject* parent) :
    iscore::PanelPresenter {view, parent}
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
