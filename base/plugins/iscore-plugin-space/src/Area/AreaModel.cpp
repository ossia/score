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


void AreaModel::setSpaceMapping(const SpaceMap& mapping)
{
    m_spaceMap = mapping;
    emit areaChanged(m_currentParameterMap);
}

void AreaModel::setParameterMapping(const ParameterMap &parameter_mapping)
{
    using namespace GiNaC;
    m_parameterMap = parameter_mapping;

    ValMap mapping;
    for(const auto& elt : m_parameterMap.keys())
    {
        std::string name = elt.toStdString();
        auto& val = m_parameterMap[elt];
        if(val.address.device.isEmpty()) // We use the value
        {
            mapping.insert(
                        std::make_pair(
                            name,
                            State::convert::value<double>(val.value)));
        }
        else // We fetch it from the device tree
        {
            Device::Node* n = Device::try_getNodeFromAddress(m_context.devices.rootNode(), val.address);
            if(n)
            {
                mapping.insert(
                            std::make_pair(
                                name,
                                State::convert::value<double>(n->get<Device::AddressSettings>().value)));
            }
            else
            {
                mapping.insert(
                            std::make_pair(
                                name,
                                State::convert::value<double>(val.value)));
            }
        }
    }
    setCurrentMapping(mapping);
}

void AreaModel::setCurrentMapping(const ValMap& map)
{
    using namespace GiNaC;

    m_currentParameterMap = map;
    for(auto sym : map)
    {
        emit currentSymbolChanged(sym.first, sym.second);
    }

    emit areaChanged(map);
}

void AreaModel::updateCurrentMapping(
        std::string sym,
        double value)
{
    m_currentParameterMap.at(sym) = value;
    emit currentSymbolChanged(sym, value);
    emit areaChanged(m_currentParameterMap);
}


spacelib::projected_area AreaModel::projectedArea() const
{
    //return spacelib::projected_area(*m_area.get(), m_spaceMap);
}

spacelib::valued_area AreaModel::valuedArea(const GiNaC::exmap& vals) const
{
    //return spacelib::valued_area(projectedArea(), vals);
}

QString AreaModel::toString() const
{
    std::stringstream s;
    for(auto& rel : m_area->rels())
        s << static_cast<const GiNaC::ex&>(rel);
    return QString::fromStdString(s.str());
}
}
