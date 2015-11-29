#include <iscore/tools/std/StdlibWrapper.hpp>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "ObjectPath.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/serialization/JSONVisitor.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const ObjectPath& path)
{
    readFrom_vector_obj_impl(*this, path.vec());
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ObjectPath& path)
{
    writeTo_vector_obj_impl(*this, path.vec());
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const ObjectPath& path)
{
    m_obj["Identifiers"] = toJsonArray(path.vec());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ObjectPath& path)
{
    fromJsonArray(m_obj["Identifiers"].toArray(), path.vec());
}
