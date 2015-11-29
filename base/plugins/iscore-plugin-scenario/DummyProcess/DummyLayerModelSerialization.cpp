
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class DummyLayerModel;
template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const DummyLayerModel& lm)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(DummyLayerModel& lm)
{
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const DummyLayerModel& lm)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(DummyLayerModel& lm)
{
}
