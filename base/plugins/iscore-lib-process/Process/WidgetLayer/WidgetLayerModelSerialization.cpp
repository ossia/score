
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore_lib_process_export.h>
namespace WidgetLayer
{
class Layer;
}
template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Reader<DataStream>>::read(const WidgetLayer::Layer& lm)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Writer<DataStream>>::writeTo(WidgetLayer::Layer& lm)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Reader<JSONObject>>::readFromConcrete(const WidgetLayer::Layer& lm)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(WidgetLayer::Layer& lm)
{
}
