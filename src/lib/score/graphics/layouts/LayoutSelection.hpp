#pragma once
#include <functional>
#include <vector>

namespace score
{
//! Which of several units (e.g. the operators of a synth) layouts show or
//! highlight. A table whose rows can be clicked sets it; a strip_detail
//! layout, or anything else listening, follows it. Shared by the layouts
//! that name the same selection.
struct LayoutSelection
{
  int current{0};
  std::vector<std::function<void(int)>> listeners;

  //! Called now with the current index, then on each change
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
