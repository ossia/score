#include "LibraryPanelModel.hpp"
#include "LibraryPanelId.hpp"


namespace Library
{
LibraryPanelModel::LibraryPanelModel(QObject* parent) :
    iscore::PanelModel {"LibraryPanelModel", parent}
{
    this->setParent(parent);
}

int LibraryPanelModel::panelId() const
{
    return LIBRARY_PANEL_ID;
}
}
