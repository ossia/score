#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>

namespace iscore {
class PanelView;

}  // namespace iscore

class DeviceExplorerPanelPresenter final : public iscore::PanelPresenter
{
    public:
        DeviceExplorerPanelPresenter(iscore::PanelView* view,
                                     QObject* parent);

        void on_modelChanged() override;
        int panelId() const override;

};
