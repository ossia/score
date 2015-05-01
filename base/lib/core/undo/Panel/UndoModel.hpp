#pragma once
#include <iscore/plugins/panel/PanelModelInterface.hpp>
namespace iscore {
class DocumentModel;
}
class UndoModel : public iscore::PanelModelInterface
{
    public:
    UndoModel(iscore::DocumentModel *model);
    int panelId() const override;
};
