#include "AutomationLayerModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

/////// ViewModel
template<>
void Visitor<Reader<DataStream>>::readFrom(const AutomationLayerModel& lm)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AutomationLayerModel& lm)
{
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const AutomationLayerModel& lm)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AutomationLayerModel& lm)
{
}
