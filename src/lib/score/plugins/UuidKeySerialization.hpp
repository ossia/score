#pragma once
#include <score/plugins/UuidKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#if defined(SCORE_DEBUG_DELIMITERS)
SCORE_SERALIZE_DATASTREAM_DEFINE_T(template <typename T>, UuidKey<T>)
#else
// The same 16 bytes DataStreamReader writes, without one: QSettings streams
// these while flushing, which can happen after the application's components
// are gone.
template <typename T>
QDataStream& operator<<(QDataStream& stream, const UuidKey<T>& obj)
{
  stream.writeRawData(
      reinterpret_cast<const char*>(obj.impl().data), sizeof(obj.impl().data));
  return stream;
}

template <typename T>
QDataStream& operator>>(QDataStream& stream, UuidKey<T>& obj)
{
  stream.readRawData(reinterpret_cast<char*>(obj.impl().data), sizeof(obj.impl().data));
  return stream;
}
#endif
