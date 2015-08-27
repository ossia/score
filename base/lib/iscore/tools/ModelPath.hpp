#pragma once
#include <QDataStream>
#include <iscore/tools/ObjectPath.hpp>


template<typename Object>
class Path;

namespace iscore
{
namespace IDocument
{
template<typename T>
Path<T> path(const T& obj);
}
}

// A typesafe path to a model object in a Document.
template<typename Object>
class Path
{
        friend
        QDataStream& operator<< (QDataStream& stream, const Path<Object>& obj)
        {
            Visitor<Reader<DataStream>> reader(stream.device());
            reader.readFrom(obj.m_impl);
            return stream;
        }

        friend
        QDataStream& operator>> (QDataStream& stream, Path<Object>& obj)
        {
            Visitor<Writer<DataStream>> writer(stream.device());
            writer.writeTo(obj.m_impl);

            return stream;
        }

        friend bool operator==(const Path& lhs, const Path& rhs)
        {
            return lhs.m_impl == rhs.m_impl;
        }

        friend uint qHash(const Path& obj, uint seed)
        {
          return qHash(obj.m_impl, seed);
        }

        template<typename T>
        friend Path<T> iscore::IDocument::path(const T& obj);

        friend class ObjectPath;

    public:
        // Use this if it is not possible to get a path
        // (for instance because the object does not exist yet)
        struct UnsafeDynamicCreation{};
        Path(const ObjectPath& obj, UnsafeDynamicCreation): m_impl(obj) { }
        Path(ObjectPath&& obj, UnsafeDynamicCreation): m_impl(std::move(obj)) { }

        Path() = default;
        Path(const Path&) = default;
        Path(Path&&) = default;
        Path& operator=(const Path&) = default;
        Path& operator=(Path&&) = default;

        Object& find() const
        { return m_impl.find<Object>(); }

        const auto& unsafePath() const
        { return m_impl; }

        auto&& moveUnsafePath()
        { return std::move(m_impl); }


    private:
        Path(ObjectPath&& path): m_impl{std::move(path)} { }
        ObjectPath m_impl;
};

