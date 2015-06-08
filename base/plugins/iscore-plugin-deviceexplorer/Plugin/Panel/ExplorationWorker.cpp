#include "ExplorationWorker.hpp"
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>

ExplorationWorker::ExplorationWorker(DeviceInterface &dev):
    m_dev{dev}
{

}

void ExplorationWorker::process()
{
    node = m_dev.refresh();
    emit finished();
}
