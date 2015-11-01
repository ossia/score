#include "DeviceExplorerPanelFactory.hpp"
#include <core/document/DocumentPresenter.hpp>

#include "PanelBase/DeviceExplorerPanelModel.hpp"
#include "PanelBase/DeviceExplorerPanelPresenter.hpp"
#include "PanelBase/DeviceExplorerPanelView.hpp"
#include "PanelBase/DeviceExplorerPanelId.hpp"
using namespace iscore;


QString DeviceExplorerPanelFactory::panelName() const
{
    return "DeviceExplorer";
}

int DeviceExplorerPanelFactory::panelId() const
{
    return DEVICEEXPLORER_PANEL_ID;
}

iscore::PanelView* DeviceExplorerPanelFactory::makeView(iscore::View* parent)
{
    return new DeviceExplorerPanelView {parent};
}

iscore::PanelPresenter* DeviceExplorerPanelFactory::makePresenter(iscore::Presenter* parent_presenter,
        iscore::PanelView* view)
{
    return new DeviceExplorerPanelPresenter {parent_presenter, view};
}

iscore::PanelModel* DeviceExplorerPanelFactory::makeModel(DocumentModel* parent)
{
    return new DeviceExplorerPanelModel {parent};
}
