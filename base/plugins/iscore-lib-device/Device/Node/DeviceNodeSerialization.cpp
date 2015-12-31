#include <Device/Node/DeviceNode.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/VariantSerialization.hpp>

namespace Device {
struct AddressSettings;
struct DeviceSettings;
}  // namespace iscore
template <typename T> class Reader;
template <typename T> class TypeToName;
template <typename T> class Writer;

// Move me
using namespace iscore;

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const Device::DeviceExplorerNode& n)
{
    readFrom(n.m_data);
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Device::DeviceExplorerNode& n)
{
    writeTo(n.m_data);
    checkDelimiter();
}


template<> class TypeToName<Device::DeviceSettings>
{ public: static constexpr const char * name() { return "DeviceSettings"; } };

template<> class TypeToName<Device::AddressSettings>
{ public: static constexpr const char * name() { return "AddressSettings"; } };


template<>
void Visitor<Reader<JSONObject>>::readFrom(const Device::DeviceExplorerNode& n)
{
    readFrom(n.m_data);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Device::DeviceExplorerNode& n)
{
    writeTo(n.m_data);
}
