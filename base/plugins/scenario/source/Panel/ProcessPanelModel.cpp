#include "ProcessPanelModel.hpp"
#include "ProcessPanelId.hpp"

ProcessPanelModel::ProcessPanelModel(QObject* parent):
    iscore::PanelModelInterface{"ProcessPanelModel", parent}
{

}

int ProcessPanelModel::panelId() const
{
    return PROCESS_PANEL_ID;
}

void ProcessPanelModel::setNewSelection(const Selection&)
{
}
