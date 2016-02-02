#pragma once
#include <iscore/plugins/panel/PanelModel.hpp>

namespace iscore
{
    struct DocumentContext;
}

namespace Explorer
{
class DeviceExplorerModel;

class DeviceExplorerPanelModel final : public iscore::PanelModel
{
        friend class DeviceExplorerPanelPresenter;
    public:
        explicit DeviceExplorerPanelModel(
                const iscore::DocumentContext&,
                QObject* parent);

        int panelId() const override;

        DeviceExplorerModel* deviceExplorer();

    private:
        DeviceExplorerModel* m_model {};

};
}
