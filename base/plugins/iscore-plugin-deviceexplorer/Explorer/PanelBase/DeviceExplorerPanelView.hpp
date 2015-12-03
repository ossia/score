#pragma once
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/panel/PanelView.hpp>

#include <QString>

class QWidget;

namespace iscore
{
    
}
class DeviceExplorerWidget;

class DeviceExplorerPanelView final : public iscore::PanelView
{
        friend class DeviceExplorerPanelPresenter;
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;
        explicit DeviceExplorerPanelView(
                const iscore::ApplicationContext& ctx,
                QObject* parent);

        QWidget* getWidget() override;
        const QString shortcut() const override
        { return tr("Ctrl+E"); }

    private:
        DeviceExplorerWidget* m_widget {};
};
