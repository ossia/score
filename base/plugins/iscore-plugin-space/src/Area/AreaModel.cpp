#include "AreaModel.hpp"

#include <DeviceExplorer/Node/Node.hpp>
AreaModel::AreaModel(
        std::unique_ptr<spacelib::area>&& area,
        const SpaceModel& space,
        const id_type<AreaModel> & id,
        QObject *parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_space{space},
    m_area{std::move(area)}
{
    for(const auto& sym : m_area->symbols())
    {
        m_parameterMap.insert(
                    sym.get_name().c_str(),
                    {sym, {false, iscore::FullAddressSettings{}}});
    }

    // TODO we should have instead {default value, address} and default value is used if address is empty.
}


void AreaModel::setSpaceMapping(const GiNaC::exmap& mapping)
{
    m_spaceMap = mapping;
}

void AreaModel::setParameterMapping(const AreaModel::ParameterMap &mapping)
{
    m_parameterMap = mapping;
}

spacelib::projected_area AreaModel::projectedArea() const
{
    return spacelib::projected_area(*m_area.get(), m_spaceMap);
}

spacelib::valued_area AreaModel::valuedArea() const
{
    GiNaC::exmap mapping;
    for(auto& elt : m_parameterMap)
    {
        if(elt.second.first) // We use the value
        {
            mapping[elt.first] = elt.second.second.value.val.toDouble();
        }
        else // We fetch it from the device tree
        {
            ISCORE_TODO
            // What do we do if the address is not there ? Mark the area as invalid ?
        }
    }
    return spacelib::valued_area(projectedArea(), mapping);
}

void AreaModel::mapAddressToParameter(const QString& str, const iscore::FullAddressSettings& addr)
{
    // TODO how to update when a value changes ??
    // Maybe we should have a default value ?
    m_parameterMap[str].second = {false, addr};
}

void AreaModel::mapValueToParameter(const QString& str, const iscore::Value&& val)
{
    m_parameterMap[str].second.first = true;
    m_parameterMap[str].second.second.value = val;
}

QString AreaModel::toString() const
{
    std::stringstream s;
    s << static_cast<const GiNaC::ex&>(m_area->rel());
    return QString::fromStdString(s.str());
}

