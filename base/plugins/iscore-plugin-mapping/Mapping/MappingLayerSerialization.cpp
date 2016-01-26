#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
namespace Mapping
{
class LayerModel;
}
template <typename T> class Reader;
template <typename T> class Writer;

/////// ViewModel
template<>
void Visitor<Reader<DataStream>>::readFrom(const Mapping::LayerModel& lm)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Mapping::LayerModel& lm)
{
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const Mapping::LayerModel& lm)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Mapping::LayerModel& lm)
{
}
