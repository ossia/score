#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelView.hpp"
#include <core/interface/document/DocumentInterface.hpp>
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
    static_cast<InspectorPanelView*>(m_view)->setCurrentDocument(doc->presenter());
}
