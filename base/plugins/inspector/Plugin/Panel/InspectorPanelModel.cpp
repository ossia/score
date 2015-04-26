#include "InspectorPanelModel.hpp"

#include <core/document/DocumentModel.hpp>

InspectorPanelModel::InspectorPanelModel(iscore::DocumentModel* parent) :
    iscore::PanelModelInterface {"InspectorPanelModel", nullptr}
// NOTE : here we declare parent after because else for some weird reason,
// "newItemInspected" is not found...
{
    this->setParent(parent);
}

void InspectorPanelModel::setNewSelection(const Selection& s)
{
    emit selectionChanged(s);
}
