#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>

class DeviceExplorerPanelPresenter final : public iscore::PanelPresenter
{
    public:
        DeviceExplorerPanelPresenter(iscore::Presenter* parent,
                                     iscore::PanelView* view);

        virtual void on_modelChanged() override;
        int panelId() const override;

};
