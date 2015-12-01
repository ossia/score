#include <core/document/DocumentModel.hpp>

#include "InspectorPanelModel.hpp"
#include <iscore/plugins/panel/PanelModel.hpp>
#include "InspectorPanelId.hpp"

InspectorPanelModel::InspectorPanelModel(QObject* parent) :
    iscore::PanelModel {"InspectorPanelModel", nullptr}
// NOTE : here we declare parent after because else for some weird reason,
// "newItemInspected" is not found...
{
    this->setParent(parent);
}

void InspectorPanelModel::setNewSelection(const Selection& s)
{
    emit selectionChanged(s);
}

int InspectorPanelModel::panelId() const
{
    return INSPECTOR_PANEL_ID;
}
