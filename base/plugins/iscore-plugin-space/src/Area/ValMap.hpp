#pragma once
#include <boost/container/flat_map.hpp>
#include <QMap>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Device/Address/AddressSettings.hpp>

#include <string>
namespace Space
{
class DimensionModel;
using ValMap = boost::container::flat_map<std::string, double>;
using SpaceMap = QMap<Id<DimensionModel>, QString>;

using ParameterMap = QMap<QString, Device::FullAddressSettings>;
}
