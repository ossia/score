#pragma once
#include <iscore/plugins/panel/PanelModel.hpp>
namespace iscore {
class DocumentModel;
}
class UndoModel final : public iscore::PanelModel
{
    public:
        explicit UndoModel(iscore::DocumentModel *model);
        int panelId() const override;
};
