#pragma once
#include <score/model/tree/InvisibleRootNode.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QList>
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
template <typename T>
class TreePath : public QList<int>
{
private:
  using impl_type = QList<int>;

public:
  TreePath() = default;
  TreePath(const impl_type& other) : impl_type(other) {}

  TreePath(QModelIndex index)
  {
    QModelIndex iter = index;

    while (iter.isValid())
    {
      prepend(iter.row());
      iter = iter.parent();
    }
  }

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

template <typename T>
struct TSerializer<DataStream, TreePath<T>>
{
  static void readFrom(DataStream::Serializer& s, const TreePath<T>& path)
  {
    s.stream() << static_cast<const QList<int>&>(path);
  }

  static void writeTo(DataStream::Deserializer& s, TreePath<T>& path)
  {
    s.stream() >> static_cast<QList<int>&>(path);
  }
};

template <typename T>
struct TSerializer<JSONObject, TreePath<T>>
{
  static void readFrom(JSONObject::Serializer& s, const TreePath<T>& path)
  {
    s.obj[s.strings.Path] = toJsonArray(static_cast<const QList<int>&>(path));
  }

  static void writeTo(JSONObject::Deserializer& s, TreePath<T>& path)
  {
    fromJsonArray(
        s.obj[s.strings.Path].toArray(), static_cast<QList<int>&>(path));
  }
};
