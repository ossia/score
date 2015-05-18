#pragma once
#include <iscore/plugins/panel/PanelFactory.hpp>

class DeviceExplorerPanelFactory : public iscore::PanelFactory
{
    public:
        int panelId() const override;
        QString panelName() const override;


        iscore::PanelView* makeView(iscore::View*) override;

        iscore::PanelPresenter* makePresenter(
                iscore::Presenter* parent_presenter,
                iscore::PanelView* view) override;

        iscore::PanelModel* makeModel(
                iscore::DocumentModel*) override;
        iscore::PanelModel* loadModel(
                const VisitorVariant& data,
                iscore::DocumentModel* parent) override;

};
