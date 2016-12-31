#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>

namespace Scenario
{

template <typename Component_T>
class ConstraintComponent : public Component_T
{
public:
  template <typename... Args>
  ConstraintComponent(Scenario::ConstraintModel& cst, Args&&... args)
      : Component_T{std::forward<Args>(args)...}, m_constraint{cst}
  {
  }

  Scenario::ConstraintModel& constraint() const
  {
    return m_constraint;
  }

  auto& context() const { return this->system().context(); }

  template <typename Models>
  auto& models() const
  {
    static_assert(
        std::is_same<Models, Process::ProcessModel>::value,
        "Constraint component must be passed Process::ProcessModel as child.");

    return m_constraint.processes;
  }

private:
  Scenario::ConstraintModel& m_constraint;
};

template <typename System_T>
using GenericConstraintComponent
    = Scenario::ConstraintComponent<iscore::GenericComponent<System_T>>;
}
