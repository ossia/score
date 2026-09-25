#pragma once
#include <functional>
#include <vector>

namespace score
{
//! Selected index shared by the layouts naming the same selection,
//! e.g. table rows driving a strip_detail.
struct LayoutSelection
{
  int current{0};
  std::vector<std::function<void(int)>> listeners;

  //! Called immediately, then on change
  void listen(std::function<void(int)> f)
  {
    f(current);
    listeners.push_back(std::move(f));
  }

  void select(int index)
  {
    if(index == current)
      return;
    current = index;
    for(auto& f : listeners)
      f(index);
  }
};
}
