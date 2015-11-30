#include <core/document/DocumentModel.hpp>

#include "UndoModel.hpp"
#include "UndoPanelId.hpp"
#include <iscore/plugins/panel/PanelModel.hpp>

UndoModel::UndoModel(iscore::DocumentModel* model) :
    iscore::PanelModel{"UndoPanelModel", model}
{

}

int UndoModel::panelId() const
{
    return UNDO_PANEL_ID;
}
