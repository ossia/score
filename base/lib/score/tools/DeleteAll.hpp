#pragma once
#include <utility>
namespace score
{
template <typename T>
void deleteAndClear(T& container)
{
  auto tmp = std::move(container);
  container.clear();
  for (auto* e : tmp)
  {
    delete e;
  }
}
}
