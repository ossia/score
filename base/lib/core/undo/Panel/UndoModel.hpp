#pragma once
#include <iscore/plugins/panel/PanelModel.hpp>
namespace iscore {
class DocumentModel;
}
class UndoModel : public iscore::PanelModel
{
    public:
    UndoModel(iscore::DocumentModel *model);
    int panelId() const override;
};
