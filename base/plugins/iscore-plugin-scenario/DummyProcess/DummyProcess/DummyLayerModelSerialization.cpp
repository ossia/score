
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore_lib_dummyprocess_export.h>
class DummyLayerModel;
template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_DUMMYPROCESS_EXPORT void Visitor<Reader<DataStream>>::readFrom(const DummyLayerModel& lm)
{
}

template<>
ISCORE_LIB_DUMMYPROCESS_EXPORT void Visitor<Writer<DataStream>>::writeTo(DummyLayerModel& lm)
{
}



template<>
ISCORE_LIB_DUMMYPROCESS_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const DummyLayerModel& lm)
{
}

template<>
ISCORE_LIB_DUMMYPROCESS_EXPORT void Visitor<Writer<JSONObject>>::writeTo(DummyLayerModel& lm)
{
}
