#include "DummyLayerModel.hpp"

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
