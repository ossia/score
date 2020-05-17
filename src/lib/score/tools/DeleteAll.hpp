#pragma once
#include <utility>
namespace score
{

template <typename T>
[[nodiscard]] auto clearAndDeleteLater(T& container) noexcept
{
  struct later
  {
    T container;

    ~later() noexcept
    {
      for (auto* e : container)
        e->deleteLater();
    }
  };

  auto tmp = std::move(container);
  for (auto ptr : tmp)
    ptr->setParent(nullptr);

  container.clear();

  // Note ! this code *requires* RVO, which is guaranteed
  // starting from C++17 in this specific case.
  // Don't put later in a variable instead !
  return later{std::move(tmp)};
}

}
