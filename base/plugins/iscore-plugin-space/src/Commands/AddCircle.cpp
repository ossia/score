#include "AddCircle.hpp"

#include "src/Area/Circle/CircleAreaModel.hpp"
#include "src/Space/SpaceModel.hpp"


#include "src/SpaceProcess.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>


AddCircle::AddCircle(
        ModelPath<SpaceProcess> &&spacProcess,
        const QMap<QString, QString> &dimMap,
        const QMap<QString, iscore::FullAddressSettings> &addrMap):
    iscore::SerializableCommand{factoryName(), commandName(), description()},
    m_path{std::move(spacProcess)},
    m_varToDimensionMap{dimMap},
    m_symbolToAddressMap{addrMap}
{
    m_createdAreaId = getStrongId(m_path.find().areas());
}

void AddCircle::undo()
{
    auto& proc = m_path.find();
    proc.removeArea(m_createdAreaId);
}

void AddCircle::redo()
{
    auto& proc = m_path.find();

    auto ar = new CircleAreaModel{proc.space(), m_createdAreaId, &proc};

    GiNaC::exmap sym_map;
    const auto& syms = ar->area().symbols();
    for(const auto& elt : m_varToDimensionMap.keys())
    {
        auto sym_it = std::find_if(syms.begin(), syms.end(),
                                   [&] (const GiNaC::symbol& sym) { return sym.get_name() == m_varToDimensionMap[elt].toStdString(); });
        Q_ASSERT(sym_it != syms.end());

        sym_map[*sym_it] = proc.space().dimension(elt).sym().symbol();
    }


    AreaModel::ParameterMap addr_map;
    for(const auto& elt : m_symbolToAddressMap.keys())
    {
        auto sym_it = std::find_if(syms.begin(), syms.end(),
                                   [&] (const GiNaC::symbol& sym) { return sym.get_name() == elt.toStdString(); });
        Q_ASSERT(sym_it != syms.end());

        addr_map[elt] = {*sym_it, m_symbolToAddressMap[elt]};
    }

    ar->setSpaceMapping(sym_map);
    ar->setParameterMapping(addr_map);

    proc.addArea(ar);
}

void AddCircle::serializeImpl(QDataStream &s) const
{
    s << m_path << m_createdAreaId << m_varToDimensionMap << m_symbolToAddressMap;
}

void AddCircle::deserializeImpl(QDataStream &s)
{
    s >> m_path >> m_createdAreaId >> m_varToDimensionMap >> m_symbolToAddressMap;
}
