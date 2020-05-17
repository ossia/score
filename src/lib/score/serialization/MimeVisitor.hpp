#pragma once
#include <score/serialization/VisitorCommon.hpp>

#include <QMimeData>

template <typename T>
struct MimeReader;
template <typename T>
struct MimeWriter;

template <typename T>
struct Mime
{
  using Serializer = MimeReader<T>;
  using Deserializer = MimeWriter<T>;

  static constexpr SerializationIdentifier type() { return 4; }
};

// Reads into a QMimeData
struct MimeDataReader
{
  QMimeData& m_mime;
  MimeDataReader(QMimeData& p) : m_mime{p} { }
};

// Writes from a QMimeData to an object.
struct MimeDataWriter
{
  const QMimeData& m_mime;
  MimeDataWriter(const QMimeData& p) : m_mime{p} { }
};
