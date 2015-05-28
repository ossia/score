#pragma once
#include <iscore/plugins/panel/PanelView.hpp>

namespace iscore
{
    class View;
}
class DeviceExplorerWidget;
class DeviceExplorerPanelView : public iscore::PanelView
{
        friend class DeviceExplorerPanelPresenter;
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;
        DeviceExplorerPanelView(iscore::View* parent);
        QWidget* getWidget() override;

    private:
        DeviceExplorerWidget* m_widget {};
};
