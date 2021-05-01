#pragma once
#include <ossia/detail/config.hpp>

class QDataStream;

#define SCORE_SERIALIZE_DATASTREAM_DECLARE(EXPORT, T)         \
  EXPORT                                                      \
  QDataStream& operator<<(QDataStream& stream, const T& obj); \
  EXPORT                                                      \
  QDataStream& operator>>(QDataStream& stream, T& obj);

#define SCORE_SERALIZE_DATASTREAM_DEFINE(T)                  \
  QDataStream& operator<<(QDataStream& stream, const T& obj) \
  {                                                          \
    DataStreamReader reader{stream.device()};                \
    reader.readFrom(obj);                                    \
    return stream;                                           \
  }                                                          \
  QDataStream& operator>>(QDataStream& stream, T& obj)       \
  {                                                          \
    DataStreamWriter writer{stream.device()};                \
    writer.writeTo(obj);                                     \
    return stream;                                           \
  }

#define SCORE_SERALIZE_DATASTREAM_DEFINE_T(TEMPLATE, T)      \
  TEMPLATE                                                   \
  QDataStream& operator<<(QDataStream& stream, const T& obj) \
  {                                                          \
    DataStreamReader reader{stream.device()};                \
    reader.readFrom(obj);                                    \
    return stream;                                           \
  }                                                          \
  TEMPLATE                                                   \
  QDataStream& operator>>(QDataStream& stream, T& obj)       \
  {                                                          \
    DataStreamWriter writer{stream.device()};                \
    writer.writeTo(obj);                                     \
    return stream;                                           \
  }
