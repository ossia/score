#pragma once
#include <iscore/plugins/panel/PanelFactoryInterface.hpp>

class DeviceExplorerPanelFactory : public iscore::PanelFactoryInterface
{
    public:
        //TODO removeme
        QString name() const override { return "DeviceExplorerPanelModel"; }


        iscore::PanelViewInterface* makeView(iscore::View*) override;

        iscore::PanelPresenterInterface* makePresenter(
                iscore::Presenter* parent_presenter,
                iscore::PanelViewInterface* view) override;

        iscore::PanelModelInterface* makeModel(
                iscore::DocumentModel*) override;
        iscore::PanelModelInterface* loadModel(
                const VisitorVariant& data,
                iscore::DocumentModel* parent) override;
};
