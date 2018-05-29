#pragma once
#include <score/model/path/ObjectPath.hpp>

namespace score
{

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

  SCORE_BREAKPOINT;
  throw std::runtime_error(
      QString("findById : id %1 not found in vector of %2")
          .arg(id)
          .arg(typeid(c).name())
          .toUtf8()
          .constData());
}

template <typename Container>
typename Container::value_type
findById_weak_unsafe(const Container& c, int32_t id) noexcept
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
