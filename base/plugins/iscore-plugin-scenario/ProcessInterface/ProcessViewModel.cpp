#include "ProcessViewModel.hpp"

ProcessModel&ProcessViewModel::sharedProcessModel() const
{ return m_sharedProcessModel; }


ProcessViewModel::ProcessViewModel(
        const id_type<ProcessViewModel>& viewModelId,
        const QString& name,
        ProcessModel& sharedProcess,
        QObject* parent) :
    IdentifiedObject<ProcessViewModel> {viewModelId, name, parent},
    m_sharedProcessModel {sharedProcess}
{

}
