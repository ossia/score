#pragma once
#include <iscore/serialization/VisitorCommon.hpp>
#include <QMimeData>

template<typename T>
struct Mime;

template<typename T>
class Visitor<Reader<Mime<T>>>;
template<typename T>
class Visitor<Writer<Mime<T>>>;

template<typename T>
struct Mime
{
    using Serializer = Visitor<Reader<Mime<T>>>;
    using Deserializer = Visitor<Writer<Mime<T>>>;

    static constexpr SerializationIdentifier type()
    { return 4; }
};

// Reads into a QMimeData
struct MimeDataReader
{
    QMimeData& m_mime;
    MimeDataReader(QMimeData& p): m_mime{p} {}
};

// Writes from a QMimeData to an object.
struct MimeDataWriter
{
    const QMimeData& m_mime;
    MimeDataWriter(const QMimeData& p): m_mime{p} {}
};
