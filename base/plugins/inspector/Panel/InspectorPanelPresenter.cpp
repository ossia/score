#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelView.hpp"
#include <core/interface/document/DocumentInterface.hpp>
#include <core/interface/selection/SelectionStack.hpp>

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
    auto doc = IDocument::documentFromObject(m_model);
    auto view = static_cast<InspectorPanelView*>(m_view);
    auto model = static_cast<InspectorPanelModel*>(m_model);
    view->setCurrentDocument(doc->presenter());
   // connect(m_model...

    disconnect(m_mvConnection);
    disconnect(m_vpConnection);

    m_mvConnection = connect(model, &InspectorPanelModel::selectionChanged,
                             view,  &InspectorPanelView::setNewSelection);
    m_vpConnection = connect(view, &InspectorPanelView::newSelection,
                             &doc->presenter()->selectionStack(), &SelectionStack::push);
}
