#pragma once
#include <iscore/tools/ObjectPath.hpp>

namespace iscore
{

// Relative path between two objects
struct ISCORE_LIB_BASE_EXPORT RelativePath
{
  // Path to go to obj2 given obj1
  // For now it will *only* work if A and B share a common ancestor and are
  // parents of each other.
  RelativePath() = default;
  RelativePath(QObject* obj1, QObject* obj2);
  RelativePath(QObject& obj1, QObject& obj2) : RelativePath{&obj1, &obj2}
  {
  }

  QObject* find_impl(QObject* source) const;

  template <typename T>
  T& find(QObject* source)
  {
    return *safe_cast<T*>(find_impl(source));
  }
  template <typename T>
  T* try_find(QObject* source)
  {
    return dynamic_cast<T*>(find_impl(source));
  }

  int32_t m_parents = 0; // Number of time we have to call parent()
  ObjectPath m_remainder;
};

// MOVEME
template <typename Container>
typename Container::value_type
findById_weak_safe(const Container& c, int32_t id)
{
  auto it = std::find_if(
      std::begin(c), std::end(c), [&id](typename Container::value_type model) {
        return model->id_val() == id;
      });

  if (it != std::end(c))
  {
    return *it;
  }

  ISCORE_BREAKPOINT;
  throw std::runtime_error(
      QString("findById : id %1 not found in vector of %2")
          .arg(id)
          .arg(typeid(c).name())
          .toUtf8()
          .constData());
}

template <typename Container>
typename Container::value_type
findById_weak_unsafe(const Container& c, int32_t id)
{
  auto it = std::find_if(
      std::begin(c), std::end(c), [&id](typename Container::value_type model) {
        return model->id_val() == id;
      });

  if (it != std::end(c))
  {
    return *it;
  }

  return nullptr;
}
}
