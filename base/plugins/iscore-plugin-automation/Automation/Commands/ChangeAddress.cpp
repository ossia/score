#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <QString>
#include <QStringList>
#include <algorithm>

#include <Automation/AutomationModel.hpp>
#include "ChangeAddress.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/Domain.hpp>
#include <State/Address.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <ossia/editor/value/value_conversion.hpp>

namespace Automation
{
ChangeAddress::ChangeAddress(
        const ProcessModel& autom,
        const State::AddressAccessor &newval):
    m_path{autom},
    m_old{autom.address(),
          autom.min(),
          autom.max()},
    m_new(Explorer::makeFullAddressAccessorSettings(
              newval,
              iscore::IDocument::documentContext(autom), 0., 1.))
{
}

ChangeAddress::ChangeAddress(
        const ProcessModel& autom,
        const Device::FullAddressSettings& newval):
    m_path{autom}
{
    m_new.address = newval.address;
    m_new.domain = newval.domain;
    m_new.address.qualifiers.unit = newval.unit;

    m_old.address = autom.address();
    m_old.domain = ossia::net::make_domain(autom.min(), autom.max());
}


void ChangeAddress::undo() const
{
    auto& autom = m_path.find();

    {
        //QSignalBlocker blck{autom.curve()};
        autom.setMin(ossia::convert<double>(ossia::net::get_min(m_old.domain)));
        autom.setMax(ossia::convert<double>(ossia::net::get_max(m_old.domain)));
        autom.setAddress(m_old.address);
    }
    // autom.curve().changed();

}

void ChangeAddress::redo() const
{
    auto& autom = m_path.find();

    {
        //QSignalBlocker blck{autom.curve()};
      autom.setMin(ossia::convert<double>(ossia::net::get_min(m_new.domain)));
      autom.setMax(ossia::convert<double>(ossia::net::get_max(m_new.domain)));
        autom.setAddress(m_new.address);
    }
    // autom.curve().changed();
}

void ChangeAddress::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_old << m_new;
}

void ChangeAddress::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_old >> m_new;
}
}
