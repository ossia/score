
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore_lib_dummyprocess_export.h>
namespace Dummy
{
class DummyLayerModel;
}
template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_DUMMYPROCESS_EXPORT void Visitor<Reader<DataStream>>::readFrom_impl(
        const Dummy::DummyLayerModel& lm)
{
}

template<>
ISCORE_LIB_DUMMYPROCESS_EXPORT void Visitor<Writer<DataStream>>::writeTo(
        Dummy::DummyLayerModel& lm)
{
}



template<>
ISCORE_LIB_DUMMYPROCESS_EXPORT void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Dummy::DummyLayerModel& lm)
{
}

template<>
ISCORE_LIB_DUMMYPROCESS_EXPORT void Visitor<Writer<JSONObject>>::writeTo(
        Dummy::DummyLayerModel& lm)
{
}
