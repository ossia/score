#pragma once
#include <eggs/variant.hpp>
#include <score/model/tree/InvisibleRootNode.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

namespace score
{

/**
 * @brief The VariantBasedNode class
 *
 * A node which can hold a single data element at the time.
 * All the arguments passed to Args are potential data member.
 *
 * Additionally, a special tag InvisibleRootNodeTag is added to serve as root
 * element, since this is necessary in the case of QAbstractItemModel.
 *
 * For instance, VariantBasedNode<int, QString> will have three possible data
 * types: InvisibleRootNode, int, QString.
 */
template <typename... Args>
class VariantBasedNode
{
public:
  VariantBasedNode(const VariantBasedNode& t) = default;
  VariantBasedNode(VariantBasedNode&& t) noexcept = default;
  VariantBasedNode& operator=(const VariantBasedNode& t) = default;

  VariantBasedNode() : m_data{InvisibleRootNode{}}
  {
  }

  template <typename T>
  VariantBasedNode(const T& t) : m_data{t}
  {
  }

  /**
   * @brief is Checks the type of the node.
   *
   * @return true if T is the currently stored type.
   */
  template <typename T>
  bool is() const
  {
    return m_data.template target<T>() != nullptr;
  }

  template <typename T>
  const T* target() const
  {
    return m_data.template target<T>();
  }

  template <typename T>
  T* target()
  {
    return m_data.template target<T>();
  }

  template <typename T>
  void set(const T& t)
  {
    m_data = t;
  }

  template <typename T>
  const T& get() const
  {
    return *m_data.template target<T>();
  }

  template <typename T>
  T& get()
  {
    return *m_data.template target<T>();
  }

  auto which() const
  {
    return m_data.which();
  }

  auto& impl() const
  {
    return m_data;
  }

  bool operator==(const VariantBasedNode& other) const
  {
    return m_data == other.m_data;
  }

protected:
  eggs::variant<InvisibleRootNode, Args...> m_data;
};
}
