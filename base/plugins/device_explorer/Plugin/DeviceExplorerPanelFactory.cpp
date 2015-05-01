#include "DeviceExplorerPanelFactory.hpp"
#include <core/document/DocumentPresenter.hpp>

#include <Singletons/DeviceExplorerInterface.hpp>

#include "PanelBase/DeviceExplorerPanelModel.hpp"
#include "PanelBase/DeviceExplorerPanelPresenter.hpp"
#include "PanelBase/DeviceExplorerPanelView.hpp"
using namespace iscore;

//@todo split this in multiple files.





iscore::PanelViewInterface* DeviceExplorerPanelFactory::makeView(iscore::View* parent)
{
    return new DeviceExplorerPanelView {parent};
}

iscore::PanelPresenterInterface* DeviceExplorerPanelFactory::makePresenter(iscore::Presenter* parent_presenter,
        iscore::PanelViewInterface* view)
{
    return new DeviceExplorerPanelPresenter {parent_presenter, view};
}

iscore::PanelModelInterface* DeviceExplorerPanelFactory::makeModel(DocumentModel* parent)
{
    return new DeviceExplorerPanelModel {parent};
}

PanelModelInterface *DeviceExplorerPanelFactory::loadModel(const VisitorVariant& data, DocumentModel *parent)
{
    return new DeviceExplorerPanelModel {data, parent};
}
