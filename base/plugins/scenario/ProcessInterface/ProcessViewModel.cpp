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


std::tuple<int, int, int> identifierOfProcessViewModelFromConstraint(ProcessViewModel* pvm)
{
    // TODO - this only works in a scenario.
    auto deckModel = static_cast<IdentifiedObjectAbstract*>(pvm->parent());
    auto boxModel = static_cast<IdentifiedObjectAbstract*>(deckModel->parent());
    return std::tuple<int, int, int>
    {
        boxModel->id_val(),
                deckModel->id_val(),
                pvm->id_val()
    };
}


QDataStream&operator<<(QDataStream& s, const std::tuple<int, int, int>& tuple)
{
    s << std::get<0> (tuple) << std::get<1> (tuple) << std::get<2> (tuple);
    return s;
}


QDataStream&operator>>(QDataStream& s, std::tuple<int, int, int>& tuple)
{
    s >> std::get<0> (tuple) >> std::get<1> (tuple) >> std::get<2> (tuple);
    return s;
}
