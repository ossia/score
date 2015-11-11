#include "AreaModel.hpp"
#include "AreaPresenter.hpp"
#include <sstream>

#include <Device/Node/DeviceNode.hpp>
AreaModel::AreaModel(
        std::unique_ptr<spacelib::area>&& area,
        const SpaceModel& space,
        const Id<AreaModel> & id,
        QObject *parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_space{space},
    m_area{std::move(area)}
{
    /*
    for(const auto& sym : m_area->symbols())
    {
        m_parameterMap.insert(
                    sym.get_name().c_str(),
                    {sym, iscore::FullAddressSettings{}});
    }
    */
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
    for(const auto& elt : m_parameterMap)
    {
        if(elt.second.address.device.isEmpty()) // We use the value
        {
            mapping[elt.first] = iscore::convert::value<double>(elt.second.value);
        }
        else // We fetch it from the device tree
        {
            ISCORE_TODO;
        }
    }
    return spacelib::valued_area(projectedArea(), mapping);
}

QString AreaModel::toString() const
{
    std::stringstream s;
    s << static_cast<const GiNaC::ex&>(m_area->rel());
    return QString::fromStdString(s.str());
}

