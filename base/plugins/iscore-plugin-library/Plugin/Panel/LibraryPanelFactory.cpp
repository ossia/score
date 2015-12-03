#include "LibraryPanelFactory.hpp"
#include "LibraryPanelView.hpp"
#include "LibraryPanelModel.hpp"
#include "LibraryPanelPresenter.hpp"
#include "LibraryPanelId.hpp"
using namespace iscore;


int LibraryPanelFactory::panelId() const
{
    return LIBRARY_PANEL_ID;
}

QString LibraryPanelFactory::panelName() const
{
    return "Library";
}

iscore::PanelView* LibraryPanelFactory::makeView(QObject* parent)
{
    return new LibraryPanelView {parent};
}

iscore::PanelPresenter* LibraryPanelFactory::makePresenter(
    iscore::Presenter* parent_presenter,
    iscore::PanelView* view)
{
    return new LibraryPanelPresenter {parent_presenter, view};
}

iscore::PanelModel* LibraryPanelFactory::makeModel(iscore::DocumentModel* parent)
{
    return new LibraryPanelModel {parent};
}

