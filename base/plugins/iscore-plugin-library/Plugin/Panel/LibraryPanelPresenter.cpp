#include "LibraryPanelPresenter.hpp"
#include "LibraryPanelModel.hpp"
#include "LibraryPanelView.hpp"
#include "LibraryPanelId.hpp"

LibraryPanelPresenter::LibraryPanelPresenter(iscore::Presenter* parent,
        iscore::PanelView* view) :
    iscore::PanelPresenter {parent, view}
{

}

int LibraryPanelPresenter::panelId() const
{
    return LIBRARY_PANEL_ID;
}

void LibraryPanelPresenter::on_modelChanged()
{
    using namespace iscore;
    if(model())
    {/*
        auto doc = IDocument::documentFromObject(model());
        auto panelview = static_cast<LibraryPanelView *>(view());
        auto panelmodel = static_cast<LibraryPanelModel *>(model());
        panelview->setCurrentDocument(doc);

        disconnect(m_mvConnection);
        m_mvConnection = connect(panelmodel, &LibraryPanelModel::selectionChanged,
                                 panelview, &LibraryPanelView::setNewSelection);
                                 */
    }
    else
    {
        /*
        auto panelview = static_cast<LibraryPanelView *>(view());
        panelview->setCurrentDocument(nullptr);

        disconnect(m_mvConnection);
        */
    }
}
