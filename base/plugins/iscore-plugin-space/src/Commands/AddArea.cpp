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

#include <src/Area/Circle/CircleAreaModel.hpp>
#include <src/Area/Pointer/PointerAreaModel.hpp>

namespace Space
{
template<typename K>
struct KeyPair : std::pair<K, K>
{
    public:
        using std::pair<K, K>::pair;

        friend bool operator==(
                const KeyPair& lhs,
                const KeyPair& rhs)
        {
            return (lhs.first == rhs.first && lhs.second == rhs.second)
                    ||
                   (lhs.first == rhs.second && lhs.second == rhs.first);
        }

        friend bool operator!=(
                const KeyPair& lhs,
                const KeyPair& rhs)
        {
            return !(lhs == rhs);
        }
};
template<typename K>
auto make_keys(const K& k1, const K& k2)
{
    return KeyPair<K>{k1, k2};
}

using CollisionFun = std::function<bool(const AreaModel& a1, const AreaModel& a2)>;

class CollisionHandler
{
        std::map<KeyPair<AreaFactoryKey>, CollisionFun> m_handlers;
    public:
        CollisionHandler()
        {
            m_handlers.insert(
                        std::make_pair(
                        make_keys(
                                CircleAreaModel::static_factoryKey(),
                                CircleAreaModel::static_factoryKey()),
                              [] (const AreaModel& a1, const AreaModel& a2)
            {
                auto& c1 = static_cast<const CircleAreaModel&>(a1);
                auto& c2 = static_cast<const CircleAreaModel&>(a2);

                auto c1_val = c1.mapToData(c1.currentMapping(), c1.parameterMapping());
                auto c2_val = c2.mapToData(c2.currentMapping(), c2.parameterMapping());

                // Check if the distance of both centers is < to the sum of radiuses
                auto dist = [] (auto v1, auto v2) {
                    return std::sqrt(std::pow(v2.x - v1.x, 2) + std::pow(v2.y - v1.y, 2));
                };
                return dist(c1_val, c2_val) < (c1_val.r + c2_val.r);
            }));

            m_handlers.insert(
                        std::make_pair(
                        make_keys(
                                CircleAreaModel::static_factoryKey(),
                                PointerAreaModel::static_factoryKey()),
                              [] (const AreaModel& a1, const AreaModel& a2)
            {
                auto& c = static_cast<const CircleAreaModel&>(a1);
                auto& p = static_cast<const PointerAreaModel&>(a2);

                auto c_val = c.mapToData(c.currentMapping(), c.parameterMapping());
                auto p_val = p.mapToData(p.currentMapping(), p.parameterMapping());

                // Check if the distance of both centers is < to the sum of radiuses
                auto dist = [] (auto v1, auto v2) {
                    return std::sqrt(std::pow(v2.x - v1.x, 2) + std::pow(v2.y - v1.y, 2));
                };
                return dist(c_val, p_val) < c_val.r;
            }));
        }

        void inscribe(std::pair<KeyPair<AreaFactoryKey>, CollisionFun> val)
        {
            m_handlers.insert(val);
        }

        bool check(const AreaModel& a1, const AreaModel& a2)
        {
            auto it = m_handlers.find(make_keys(a1.factoryKey(), a2.factoryKey()));
            if(it != m_handlers.end())
            {
                return it->second(a1, a2);
            }

            // TODO return a generic computation

            // TODO for solving collisions, put the equations in a system ?
            return false;
        }
};


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

    /// temporarily create "collision" computations
    for(auto& area : proc.areas)
    {
        { // collisions
            auto comp = new ComputationModel([&,ar] () {
                CollisionHandler h;
                return (double) h.check(area, *ar);
            },
                proc.space(),
                getStrongId(proc.computations),
                &proc);

            comp->metadata.setName(QString("coll_%1_%2")
                                   .arg(area.metadata.name())
                                   .arg(ar->metadata.name()));

            proc.computations.add(comp);
        }
        // TODO distance ?
        // TODO js ?
    }


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
}
