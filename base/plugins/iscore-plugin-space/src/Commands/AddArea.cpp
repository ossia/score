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

AddArea::AddArea(Path<SpaceProcess> &&spacProcess,
                 int areatype,
                 const QString &area,
                 const QMap<Id<DimensionModel>, QString> &dimMap,
                 const QMap<QString, iscore::FullAddressSettings> &addrMap):
    m_path{std::move(spacProcess)},
    m_areaType{areatype},
    m_areaFormula{area},
    m_dimensionToVarMap{dimMap},
    m_symbolToAddressMap{addrMap}
{
    m_createdAreaId = getStrongId(m_path.find().areas());
}

void AddArea::undo() const
{
    auto& proc = m_path.find();
    proc.removeArea(m_createdAreaId);
}

void AddArea::redo() const
{
    auto& proc = m_path.find();

    const auto& facts = context.components.factory<SingletonAreaFactoryList>().get();
    auto it = boost::range::find_if(facts,
                                    [&] (const auto& f) { return f.second->type() == m_areaType; });

    ISCORE_ASSERT(it != facts.end());

    auto ar = it->second->makeModel(m_areaFormula, proc.space(), m_createdAreaId, &proc);

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

    proc.addArea(ar);
}

void AddArea::serializeImpl(DataStreamInput &s) const
{
    s << m_path << m_createdAreaId << m_areaType << m_areaFormula << m_dimensionToVarMap << m_symbolToAddressMap;
}

void AddArea::deserializeImpl(DataStreamOutput &s)
{
    s >> m_path >> m_createdAreaId >> m_areaType >> m_areaFormula >> m_dimensionToVarMap >> m_symbolToAddressMap;
}
