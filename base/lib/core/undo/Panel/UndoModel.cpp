#include <core/document/DocumentModel.hpp>

#include "UndoModel.hpp"
#include "UndoPanelId.hpp"
#include <iscore/plugins/panel/PanelModel.hpp>

UndoModel::UndoModel(QObject* parent) :
    iscore::PanelModel{"UndoPanelModel", parent}
{

}

int UndoModel::panelId() const
{
    return UNDO_PANEL_ID;
}
