#pragma once
#include <iscore/plugins/panel/PanelModel.hpp>

namespace iscore
{
    class DocumentModel;
}
class DeviceExplorerModel;

class DeviceExplorerPanelModel : public iscore::PanelModel
{
        friend class DeviceExplorerPanelPresenter;
    public:
        DeviceExplorerPanelModel(iscore::DocumentModel* parent);

        int panelId() const override;

        DeviceExplorerModel* deviceExplorer();

    private:
        DeviceExplorerModel* m_model {};

};
