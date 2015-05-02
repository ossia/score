#include "ProcessViewModelInterface.hpp"

ProcessSharedModelInterface&ProcessViewModelInterface::sharedProcessModel() const
{ return m_sharedProcessModel; }


ProcessViewModelInterface::ProcessViewModelInterface(
        const id_type<ProcessViewModelInterface>& viewModelId,
        const QString& name,
        ProcessSharedModelInterface& sharedProcess,
        QObject* parent) :
    IdentifiedObject<ProcessViewModelInterface> {viewModelId, name, parent},
    m_sharedProcessModel {sharedProcess}
{

}


std::tuple<int, int, int> identifierOfProcessViewModelFromConstraint(ProcessViewModelInterface* pvm)
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
