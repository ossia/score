#pragma once
#include <QDataStream>
#include <iscore/tools/ObjectPath.hpp>


template<typename Object>
class ModelPath;

namespace iscore
{
namespace IDocument
{
template<typename T>
ModelPath<T> safe_path(const T& obj);
}
}

// A typesafe path to a model object in a Document.
template<typename Object>
class ModelPath
{
        friend
        QDataStream& operator<< (QDataStream& stream, const ModelPath<Object>& obj)
        {
            Visitor<Reader<DataStream>> reader(stream.device());
            reader.readFrom(obj.m_impl);
            return stream;
        }

        friend
        QDataStream& operator>> (QDataStream& stream, ModelPath<Object>& obj)
        {
            Visitor<Writer<DataStream>> writer(stream.device());
            writer.writeTo(obj.m_impl);

            return stream;
        }

        template<typename T>
        friend ModelPath<T> iscore::IDocument::safe_path(const T& obj);

    public:
        // Use this if it is not possible to get a path
        // (for instance because the object does not exist yet)
        struct UnsafeDynamicCreation{};
        ModelPath(const ObjectPath& obj, UnsafeDynamicCreation): m_impl(obj) { }
        ModelPath(ObjectPath&& obj, UnsafeDynamicCreation): m_impl(std::move(obj)) { }

        ModelPath() = default;
        ModelPath(const ModelPath&) = default;
        ModelPath(ModelPath&&) = default;
        ModelPath& operator=(const ModelPath&) = default;
        ModelPath& operator=(ModelPath&&) = default;

        Object& find() const
        { return m_impl.find<Object>(); }


    private:
        ModelPath(ObjectPath&& path): m_impl{std::move(path)} { }
        ObjectPath m_impl;
};

