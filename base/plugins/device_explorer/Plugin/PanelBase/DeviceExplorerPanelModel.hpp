#pragma once
#include <iscore/plugins/panel/PanelModelInterface.hpp>

namespace iscore
{
    class DocumentModel;
}
class DeviceExplorerModel;

class DeviceExplorerPanelModel : public iscore::PanelModelInterface
{
        friend class DeviceExplorerPanelPresenter;
    public:

        DeviceExplorerPanelModel(iscore::DocumentModel* parent);
        DeviceExplorerPanelModel(const VisitorVariant& data, iscore::DocumentModel* parent);

        void serialize(const VisitorVariant&) const override;

        DeviceExplorerModel* deviceExplorer();

    private:
        DeviceExplorerModel* m_model {};

};
