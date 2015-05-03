#include "UndoModel.hpp"
#include <core/document/DocumentModel.hpp>
#include "UndoPanelId.hpp"

UndoModel::UndoModel(iscore::DocumentModel* model) :
    iscore::PanelModel{"UndoPanelModel", model}
{

}

int UndoModel::panelId() const
{
    return UNDO_PANEL_ID;
}
