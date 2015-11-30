#include "ProcessPanelModel.hpp"
#include <iscore/plugins/panel/PanelModel.hpp>
#include "ProcessPanelId.hpp"

class QObject;

ProcessPanelModel::ProcessPanelModel(QObject* parent):
    iscore::PanelModel{"ProcessPanelModel", parent}
{

}

int ProcessPanelModel::panelId() const
{
    return PROCESS_PANEL_ID;
}

void ProcessPanelModel::setNewSelection(const Selection&)
{
}
