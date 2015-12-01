#include "LibraryPanelModel.hpp"
#include "LibraryPanelId.hpp"


LibraryPanelModel::LibraryPanelModel(iscore::DocumentModel* parent) :
    iscore::PanelModel {"LibraryPanelModel", parent}
{
    this->setParent(parent);
}

int LibraryPanelModel::panelId() const
{
    return LIBRARY_PANEL_ID;
}
