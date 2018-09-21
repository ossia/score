#pragma once
#include <score/model/tree/TreeNode.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <typename T>
struct TSerializer<DataStream, TreeNode<T>>
{
  static void readFrom(DataStream::Serializer& s, const TreeNode<T>& n)
  {
    s.readFrom(static_cast<const T&>(n));

    s.stream() << n.childCount();
    for (const auto& child : n)
    {
      s.readFrom(child);
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, TreeNode<T>& n)
  {
    s.writeTo(static_cast<T&>(n));

    int childCount;
    s.stream() >> childCount;
    for (int i = 0; i < childCount; ++i)
    {
      TreeNode<T> child;
      s.writeTo(child);
      n.push_back(std::move(child));
    }

    s.checkDelimiter();
  }
};

template <typename T>
struct TSerializer<JSONObject, TreeNode<T>>
{
  static void readFrom(JSONObject::Serializer& s, const TreeNode<T>& n)
  {
    s.readFrom(static_cast<const T&>(n));
    s.obj[s.strings.Children] = toJsonArray(n);
  }

  static void writeTo(JSONObject::Deserializer& s, TreeNode<T>& n)
  {
    s.writeTo(static_cast<T&>(n));
    auto children = s.obj[s.strings.Children].toArray();
    for (const auto& val : children)
    {
      TreeNode<T> child;
      JSONObject::Deserializer nodeWriter(val.toObject());

      nodeWriter.writeTo(child);
      n.push_back(std::move(child));
    }
  }
};
