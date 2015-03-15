#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelView.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/selection/SelectionStack.hpp>

#include <core/document/DocumentPresenter.hpp>
#include <core/document/Document.hpp>
InspectorPanelPresenter::InspectorPanelPresenter(iscore::Presenter* parent,
        iscore::PanelViewInterface* view) :
    iscore::PanelPresenterInterface {parent, view}
{

}

void InspectorPanelPresenter::on_modelChanged()
{
    using namespace iscore;
    auto doc = IDocument::documentFromObject(model());
    auto panelview = static_cast<InspectorPanelView*>(view());
    auto panelmodel = static_cast<InspectorPanelModel*>(model());
    panelview->setCurrentDocument(doc);

    disconnect(m_mvConnection);
    m_mvConnection = connect(panelmodel, &InspectorPanelModel::selectionChanged,
                             panelview,  &InspectorPanelView::setNewSelection);
}
