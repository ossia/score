#include "ProcessPanelModel.hpp"


ProcessPanelModel::ProcessPanelModel(QObject* parent):
    iscore::PanelModelInterface{"ProcessPanelModel", parent}
{

}

void ProcessPanelModel::setNewSelection(const Selection&)
{
}
