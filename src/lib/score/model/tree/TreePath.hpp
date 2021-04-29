#pragma once
#include <score/model/tree/InvisibleRootNode.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <vector>
#include <QModelIndex>
template <typename T>
using ref = T&;
template <typename T>
using cref = const T&;

enum class InsertMode
{
  AsSibling,
  AsChild
};

/**
 * @class TreePath
 * @brief Path in a tree of QAbstractItemModel objects
 *
 * Represents a path in a tree made with TreeNode.
 * This allows sending path in commands and over the network.
 *
 * TODO : it should be feasible to add the caching of a QModelIndex or
 * something like this here.
 */

// Sadly we can't have a non-const interface
// because of QList<Node*> in Node::children...
class TreePath : public std::vector<int>
{
private:
  using impl_type = std::vector<int>;

public:
  TreePath() = default;
  TreePath(const impl_type& other) : impl_type(other) { }

  TreePath(QModelIndex index)
  {
    QModelIndex iter = index;

    if(iter.isValid())
      reserve(4);

    while (iter.isValid())
    {
      prepend(iter.row());
      iter = iter.parent();
    }
  }

  template <typename T>
  TreePath(const T& node)
  {
    // We have to take care of the root node.
    if (!node.parent())
      return;

    auto iter = &node;
    while (iter && iter->parent())
    {
      prepend(iter->parent()->indexOfChild(iter));
      iter = iter->parent();
    }
  }

  void prepend(int val)
  {
    this->insert(this->begin(), val);
  }

  template <typename T>
  const T* toNode(const T* iter) const
  {
    const int pathSize = size();

    for (int i = 0; i < pathSize; ++i)
    {
      if (at(i) < iter->childCount())
      {
        iter = &iter->childAt(at(i));
      }
      else
      {
        return nullptr;
      }
    }

    return iter;
  }

  template <typename T>
  T* toNode(T* iter) const
  {
    const int pathSize = size();

    for (int i = 0; i < pathSize; ++i)
    {
      if (at(i) < iter->childCount())
      {
        iter = &iter->childAt(at(i));
      }
      else
      {
        return nullptr;
      }
    }

    return iter;
  }
};

template <>
struct is_custom_serialized<TreePath> : std::true_type
{
};

template<>
struct TSerializer<DataStream, TreePath> : TSerializer<DataStream, std::vector<int>>
{
  static void readFrom(DataStream::Serializer& s, const TreePath& path)
  {
     TSerializer<DataStream, std::vector<int>>::readFrom(s, static_cast<const std::vector<int>&>(path));
  }

  static void writeTo(DataStream::Deserializer& s, TreePath& path)
  {
    TSerializer<DataStream, std::vector<int>>::writeTo(s, static_cast<std::vector<int>&>(path));
  }
};

template<>
struct TSerializer<JSONObject, TreePath>
{
  static void readFrom(JSONObject::Serializer& s, const TreePath& path)
  {
    s.obj[s.strings.Path] = static_cast<const std::vector<int>&>(path);
  }

  static void writeTo(JSONObject::Deserializer& s, TreePath& path)
  {
    static_cast<std::vector<int>&>(path) <<= s.obj[s.strings.Path];
  }
};
