#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class AutomationLayerModel;
template <typename T> class Reader;
template <typename T> class Writer;

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
