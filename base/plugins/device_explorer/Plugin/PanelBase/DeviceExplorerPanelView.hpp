#pragma once
#include <iscore/plugins/panel/PanelViewInterface.hpp>

namespace iscore
{
    class View;
}
class DeviceExplorerWidget;
class DeviceExplorerPanelView : public iscore::PanelViewInterface
{
        friend class DeviceExplorerPanelPresenter;
    public:
        DeviceExplorerPanelView(iscore::View* parent);
        QWidget* getWidget() override;

        Qt::DockWidgetArea defaultDock() const override;
        int priority() const override;
        QString prettyName() const override;

    private:
        DeviceExplorerWidget* m_widget {};
};
