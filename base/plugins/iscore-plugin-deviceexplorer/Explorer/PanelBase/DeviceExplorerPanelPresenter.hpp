#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>

namespace iscore {
class PanelView;

}  // namespace iscore

namespace Explorer
{
class DeviceExplorerPanelPresenter final : public iscore::PanelPresenter
{
    public:
        DeviceExplorerPanelPresenter(iscore::PanelView* view,
                                     QObject* parent);

        void on_modelChanged(
                iscore::PanelModel* oldm,
                iscore::PanelModel* newm) override;
        int panelId() const override;

};
}
