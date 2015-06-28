#include "LibraryPanelModel.hpp"
#include "LibraryPanelId.hpp"

#include <core/document/DocumentModel.hpp>

LibraryPanelModel::LibraryPanelModel(iscore::DocumentModel* parent) :
    iscore::PanelModel {"LibraryPanelModel", parent}
{
    this->setParent(parent);
}

int LibraryPanelModel::panelId() const
{
    return LIBRARY_PANEL_ID;
}

void LibraryPanelModel::serialize(const VisitorVariant&) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}
