#pragma once
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class DeviceExplorerPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        DeviceExplorerPanelPresenter(iscore::Presenter* parent,
                                     iscore::PanelViewInterface* view);

        virtual void on_modelChanged() override;
        QString modelObjectName() const override;

};
