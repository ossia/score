#pragma once
#include <score/tools/std/IndirectContainer.hpp>

#include <memory>
#include <vector>

template <
    template <class, class>
    class Container,
    typename T,
    typename U = std::allocator<std::unique_ptr<T>>>
class PtrContainer : Container<std::unique_ptr<T>, U>
{
public:
  using ctnr_t = Container<std::unique_ptr<T>, U>;
  using ctnr_t::ctnr_t;
  using ctnr_t::emplace_back;

  auto begin() { return score::make_indirect_iterator(ctnr_t::begin()); }
  auto end() { return score::make_indirect_iterator(ctnr_t::end()); }
  auto begin() const { return score::make_indirect_iterator(ctnr_t::begin()); }
  auto end() const { return score::make_indirect_iterator(ctnr_t::end()); }
  auto cbegin() const
  {
    return score::make_indirect_iterator(ctnr_t::cbegin());
  }
  auto cend() const { return score::make_indirect_iterator(ctnr_t::cend()); }
};

template <typename T>
using OwningVector = PtrContainer<std::vector, T>;
