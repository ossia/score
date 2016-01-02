#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
template <typename T> class Reader;
template <typename T> class Writer;

namespace Automation
{
class LayerModel;
}
/////// ViewModel
template<>
void Visitor<Reader<DataStream>>::readFrom(const Automation::LayerModel& lm)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Automation::LayerModel& lm)
{
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const Automation::LayerModel& lm)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Automation::LayerModel& lm)
{
}
