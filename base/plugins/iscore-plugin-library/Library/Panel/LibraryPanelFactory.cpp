#include "LibraryPanelFactory.hpp"
#include "LibraryPanelView.hpp"
#include "LibraryPanelModel.hpp"
#include "LibraryPanelPresenter.hpp"
#include "LibraryPanelId.hpp"
using namespace iscore;


namespace Library
{
int LibraryPanelFactory::panelId() const
{
    return LIBRARY_PANEL_ID;
}

QString LibraryPanelFactory::panelName() const
{
    return "Library";
}


iscore::PanelView* LibraryPanelFactory::makeView(
        const iscore::ApplicationContext& ctx,
        QObject* parent)
{
    return new LibraryPanelView {parent};
}

iscore::PanelPresenter* LibraryPanelFactory::makePresenter(
        const iscore::ApplicationContext& ctx,
        iscore::PanelView* view,
        QObject* parent)
{
    return new LibraryPanelPresenter {view, parent};
}

iscore::PanelModel* LibraryPanelFactory::makeModel(
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new LibraryPanelModel {parent};
}
}

