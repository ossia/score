#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>

namespace iscore {
class PanelView;
class Presenter;
}  // namespace iscore

class DeviceExplorerPanelPresenter final : public iscore::PanelPresenter
{
    public:
        DeviceExplorerPanelPresenter(iscore::Presenter* parent,
                                     iscore::PanelView* view);

        void on_modelChanged() override;
        int panelId() const override;

};
