#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

// Move me
using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const InvisibleRootNodeTag& n)
{
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(InvisibleRootNodeTag& n)
{
    checkDelimiter();
}
// Move me
using namespace iscore;
template<>
void Visitor<Reader<JSONObject>>::readFrom(const InvisibleRootNodeTag& n)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(InvisibleRootNodeTag& n)
{
}



using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const DeviceExplorerNode& n)
{
    readFrom(n.m_data);
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(DeviceExplorerNode& n)
{
    writeTo(n.m_data);
    checkDelimiter();
}


template<> class TypeToName<iscore::DeviceSettings>
{ public: static constexpr const char * name() { return "DeviceSettings"; } };

template<> class TypeToName<iscore::AddressSettings>
{ public: static constexpr const char * name() { return "AddressSettings"; } };

template<> class TypeToName<InvisibleRootNodeTag>
{ public: static constexpr const char * name() { return "RootNode"; } };

template<>
void Visitor<Reader<JSONObject>>::readFrom(const DeviceExplorerNode& n)
{
    readFrom(n.m_data);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(DeviceExplorerNode& n)
{
    writeTo(n.m_data);
}
