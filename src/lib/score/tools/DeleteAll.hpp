#pragma once
#include <utility>
namespace score
{
template<typename T>
struct delete_later
{
  T container;

  ~delete_later() noexcept
  {
    for (auto* e : container)
      e->deleteLater();
  }
};

template <typename T>
void clearAndDeleteLater(T& container, delete_later<T>& storage) noexcept
{
  auto tmp = std::move(container);
  for (auto ptr : tmp)
    ptr->setParent(nullptr);

  container.clear();

  storage.container.insert(storage.container.end(), tmp.begin(), tmp.end());
}

template <typename T>
[[nodiscard]] auto clearAndDeleteLater(T& container) noexcept
{
  auto tmp = std::move(container);
  for (auto ptr : tmp)
    ptr->setParent(nullptr);

  container.clear();

  // Note ! this code *requires* RVO, which is guaranteed
  // starting from C++17 in this specific case.
  // Don't put later in a variable instead !
  return delete_later<T>{std::move(tmp)};
}

}
