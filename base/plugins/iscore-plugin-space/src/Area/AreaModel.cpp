#include "AreaModel.hpp"
#include "AreaPresenter.hpp"
#include <sstream>

#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

namespace Space
{
AreaModel::AreaModel(
        std::unique_ptr<spacelib::area>&& area,
        const Space::AreaContext& space,
        const Id<AreaModel> & id,
        QObject *parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_context{space},
    m_area{std::move(area)}
{
    metadata.setName(QString("Area.%1").arg(*this->id().val()));
    /*
    for(const auto& sym : m_area->symbols())
    {
        m_parameterMap.insert(
                    sym.get_name().c_str(),
                    {sym, Device::FullAddressSettings{}});
    }
    */
}


void AreaModel::setSpaceMapping(const GiNaC::exmap& mapping)
{
    m_spaceMap = mapping;
    emit areaChanged(m_currentParameterMap);
}

void AreaModel::setParameterMapping(const AreaModel::ParameterMap &parameter_mapping)
{
    m_parameterMap = parameter_mapping;

    GiNaC::exmap mapping;
    for(const auto& elt : m_parameterMap)
    {
        if(elt.second.address.device.isEmpty()) // We use the value
        {
            mapping[elt.first] = State::convert::value<double>(elt.second.value);
        }
        else // We fetch it from the device tree
        {
            Device::Node* n = Device::try_getNodeFromAddress(m_context.devices.rootNode(), elt.second.address);
            if(n)
            {
                mapping[elt.first] = State::convert::value<double>(n->get<Device::AddressSettings>().value);
            }
            else
            {
                mapping[elt.first] = State::convert::value<double>(elt.second.value);
            }
        }
    }
    setCurrentMapping(mapping);
}

void AreaModel::setCurrentMapping(const GiNaC::exmap& map)
{
    using namespace GiNaC;

    m_currentParameterMap = map;
    for(auto sym : map)
    {
        emit currentSymbolChanged(
                ex_to<symbol>(sym.first),
                ex_to<numeric>(sym.second).to_double());
    }

    emit areaChanged(map);
}

void AreaModel::updateCurrentMapping(
        const GiNaC::symbol& sym,
        double value)
{
    m_currentParameterMap.at(sym) = value;
    emit currentSymbolChanged(sym, value);
    emit areaChanged(m_currentParameterMap);
}


spacelib::projected_area AreaModel::projectedArea() const
{
    return spacelib::projected_area(*m_area.get(), m_spaceMap);
}

spacelib::valued_area AreaModel::valuedArea(const GiNaC::exmap& vals) const
{
    return spacelib::valued_area(projectedArea(), vals);
}

QString AreaModel::toString() const
{
    std::stringstream s;
    s << static_cast<const GiNaC::ex&>(m_area->rel());
    return QString::fromStdString(s.str());
}
}
