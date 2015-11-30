#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class MappingLayerModel;
template <typename T> class Reader;
template <typename T> class Writer;

/////// ViewModel
template<>
void Visitor<Reader<DataStream>>::readFrom(const MappingLayerModel& lm)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(MappingLayerModel& lm)
{
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const MappingLayerModel& lm)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(MappingLayerModel& lm)
{
}
