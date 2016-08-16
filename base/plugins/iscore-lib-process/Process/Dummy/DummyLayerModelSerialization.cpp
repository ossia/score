
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore_lib_process_export.h>
namespace Dummy
{
class Layer;
}
template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<DataStream>>::readFrom_impl(
        const Dummy::Layer& lm)
{
}

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Writer<DataStream>>::writeTo(
        Dummy::Layer& lm)
{
}



template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Dummy::Layer& lm)
{
}

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Writer<JSONObject>>::writeTo(
        Dummy::Layer& lm)
{
}
