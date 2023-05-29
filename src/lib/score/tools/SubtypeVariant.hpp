#pragma once
#include <ossia/detail/variant.hpp>

#include <boost/mp11/algorithm.hpp>

#include <type_traits>

namespace score
{
namespace detail
{
/**
 * @class dereference_visitor
 * @brief Proxy visitor that dereferences before calling the actual visitor.
 */
template <typename T>
struct dereference_visitor
{
  const T& impl;

  template <typename Arg>
  auto operator()(Arg&& arg) const
  {
    return impl(*std::forward<Arg>(arg));
  }

  auto operator()() const { return impl(); }
};

template <typename Variant, typename Base>
Variant make_subtype_variant(const Base&)
{
  // The type does not match any
  // in the proposed types for the variant.
  return Variant{};
}

template <typename Variant, typename Base, typename Arg, typename... SubArgs>
Variant make_subtype_variant(Base& base)
{
  if(auto derived = dynamic_cast<Arg*>(&base))
  {
    return Variant(derived);
  }

  return make_subtype_variant<Variant, Base, SubArgs...>(base);
}

template <typename Variant, typename Base, typename Arg, typename... SubArgs>
Variant make_subtype_variant(const Base& base)
{
  if(auto derived = dynamic_cast<const Arg*>(&base))
  {
    return Variant(derived);
  }

  return make_subtype_variant<Variant, Base, SubArgs...>(base);
}
}

/**
 * \class SubtypeVariant
 * \brief Tools to build a variant type from classes in a same hierarchy.
 *
 * This allows to restrict and optimize polymorphism for the case where
 * we already have a base class hierarchy, but we know that we will
 * be applying a particular algorithm only to specific types of
 * this hierarchy.
 */
template <typename Base, typename... Args>
class SubtypeVariant
{
  using arg_list = ossia::variant<Args...>;
  using ptr_list = boost::mp11::mp_transform<std::add_pointer_t, arg_list>;

  ptr_list m_impl;

public:
  explicit SubtypeVariant(Base& b)
      : m_impl(detail::make_subtype_variant<ptr_list, Base, Args...>(b))
  {
  }

  explicit SubtypeVariant(const Base& b)
      : m_impl(detail::make_subtype_variant<ptr_list, Base, Args...>(b))
  {
  }

  template <typename F>
  auto apply(F&& f)
  {
    if(m_impl)
    {
      return ossia::visit(detail::dereference_visitor<F>{f}, m_impl);
    }
    else
    {
      return detail::dereference_visitor<F>{f}();
    }
  }
};
}
