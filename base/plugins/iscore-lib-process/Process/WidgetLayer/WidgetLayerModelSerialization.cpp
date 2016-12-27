
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore_lib_process_export.h>
namespace WidgetLayer
{
class Layer;
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const WidgetLayer::Layer& lm)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::writeTo(WidgetLayer::Layer& lm)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::readFromConcrete(const WidgetLayer::Layer& lm)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::writeTo(WidgetLayer::Layer& lm)
{
}
