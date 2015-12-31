#include "AddArea.hpp"

#include "src/Area/AreaModel.hpp"
#include "src/Area/AreaParser.hpp"
#include "src/Space/SpaceModel.hpp"

#include "src/Area/AreaFactory.hpp"
#include "src/Area/SingletonAreaFactoryList.hpp"

#include "src/SpaceProcess.hpp"

#include <iscore/application/ApplicationContext.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

AddArea::AddArea(Path<Space::ProcessModel> &&spacProcess,
                 AreaFactoryKey type,
                 const QStringList &area,
                 const QMap<Id<DimensionModel>, QString> &dimMap,
                 const QMap<QString, Device::FullAddressSettings> &addrMap):
    m_path{std::move(spacProcess)},
    m_areaType{type},
    m_areaFormula{area},
    m_dimensionToVarMap{dimMap},
    m_symbolToAddressMap{addrMap}
{
    m_createdAreaId = getStrongId(m_path.find().areas);
}

void AddArea::undo() const
{
    auto& proc = m_path.find();
    proc.areas.remove(m_createdAreaId);
}

void AddArea::redo() const
{
    auto& proc = m_path.find();

    auto factory = context.components.factory<SingletonAreaFactoryList>().get(m_areaType);
    ISCORE_ASSERT(factory);

    auto ar = factory->makeModel(m_areaFormula, proc.context(), m_createdAreaId, &proc);

    GiNaC::exmap sym_map;
    const auto& syms = ar->area().symbols();
    for(const auto& dim : m_dimensionToVarMap.keys())
    {
        auto sym_it = std::find_if(syms.begin(), syms.end(),
                                   [&] (const GiNaC::symbol& sym)
            { return sym.get_name() == m_dimensionToVarMap[dim].toStdString(); });
        ISCORE_ASSERT(sym_it != syms.end());

        sym_map[*sym_it] = proc.space().dimension(dim).sym().symbol();
    }


    AreaModel::ParameterMap addr_map;
    for(const auto& elt : m_symbolToAddressMap.keys())
    {
        auto sym_it = std::find_if(syms.begin(), syms.end(),
                                   [&] (const GiNaC::symbol& sym) { return sym.get_name() == elt.toStdString(); });
        ISCORE_ASSERT(sym_it != syms.end());

        addr_map[elt] = {*sym_it, m_symbolToAddressMap[elt]};
    }

    ar->setSpaceMapping(sym_map);
    ar->setParameterMapping(addr_map);

    proc.areas.add(ar);
}

void AddArea::serializeImpl(DataStreamInput &s) const
{
    s << m_path << m_createdAreaId << m_areaType << m_areaFormula << m_dimensionToVarMap << m_symbolToAddressMap;
}

void AddArea::deserializeImpl(DataStreamOutput &s)
{
    s >> m_path >> m_createdAreaId >> m_areaType >> m_areaFormula >> m_dimensionToVarMap >> m_symbolToAddressMap;
}
