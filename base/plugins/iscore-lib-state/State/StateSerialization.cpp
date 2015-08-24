#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "State.hpp"
#include "Message.hpp"

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const StateData& state)
{
    readFrom(state.m_data);
    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const StateData& state)
{
    readFrom(state.m_data);
}

template<> class TypeToName<iscore::MessageList>
{ public: static constexpr const char * name() { return "MessageList"; } };

template<> class TypeToName<ProcessState>
{ public: static constexpr const char * name() { return "ProcessState"; } };

template<>
void Visitor<Writer<DataStream>>::writeTo(StateData& state)
{
    writeTo(state.m_data);
    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(StateData& state)
{
    writeTo(state.m_data);
}

