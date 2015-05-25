#pragma once

#include <iscore/plugins/panel/PanelModel.hpp>
namespace iscore
{
    class DocumentModel;
}
class LibraryPanelModel : public iscore::PanelModel
{
        Q_OBJECT
    public:
        LibraryPanelModel(iscore::DocumentModel* parent);
        int panelId() const override;

        void serialize(const VisitorVariant&) const override;
};
