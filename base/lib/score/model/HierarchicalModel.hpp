#pragma once
#include <score/model/Component.hpp>
namespace score
{
/**
 * These classes are here to manage hierarchical classes more easily
 */
template <typename Model, typename T, T Model::*ptr>
struct HierarchicalMember
{
  using model_type = Model;
  using child_type = T;
  static const constexpr auto member = ptr;
};

template <typename Model, typename... Args>
struct HierarchicModel;

template <typename Model, typename Arg, typename... Args>
struct HierarchicModel<Model, Arg, Args...>
    : public HierarchicModel<Model, Args...>
{
public:
  using HierarchicModel<Model, Args...>::HierarchicModel;

  virtual ~HierarchicModel()
  {
    auto& member = this->*Arg::ptr;
    for (auto elt : member->map().get())
    {
      delete elt;
    }
    member->map().clear();
  }
};

template <typename Model, typename Arg>
struct HierarchicModel<Model, Arg> : public Model
{
public:
  using Model::Model;

  virtual ~HierarchicModel()
  {
    auto& member = this->*Arg::ptr;
    for (auto elt : member->map().get())
    {
      delete elt;
    }
    member->map().clear();
  }
};

// Special case for the common components case :

template <typename Model>
using HierarchicalComponents
    = HierarchicalMember<Model, score::Components, &Model::components>;
}
