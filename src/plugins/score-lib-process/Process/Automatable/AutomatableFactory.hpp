#pragma once
#include <Process/ProcessFactory.hpp>
#include <State/Address.hpp>
#include <State/Value.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/common/parameter_properties.hpp>

#include <score_lib_process_export.h>

namespace ossia
{
struct unit_t;
}
namespace score
{
struct ApplicationContext;
}

namespace Process
{
/**
 * @brief Factory for processes that work like automations
 *
 * Can be used e.g. when dropping an address from the device explorer,
 * to make a gradient if it's a color, a spline if it's a 2d parameter, etc...
 */
class SCORE_LIB_PROCESS_EXPORT AutomatableFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ProcessModel, "a8d33b1e-3161-4f36-989a-cd9104388d51")

public:
  ~AutomatableFactory() override;

  //! Is this type able to provide a curve type for the following ossia type
  virtual bool matches(const ossia::complex_type& t) const noexcept = 0;

  //! Get the factory used
  virtual Process::ProcessModelFactory*
  get(const score::ApplicationContext& ctx) const noexcept = 0;

  //! Number of processes that should be created
  virtual int count(
      const State::AddressAccessor& address,
      const ossia::value& start,
      const ossia::value& end) const noexcept = 0;

  //! Instantiate the actual processes
  virtual std::vector<Process::ProcessModel*> make(
      const Process::ProcessModelFactory& factory,
      const score::DocumentContext& ctx,
      const State::AddressAccessor& address,
      const ossia::value& start,
      const ossia::value& end,
      const TimeVal& duration,
      const std::vector<Id<ProcessModel>>& id,
      QObject* parent) const noexcept = 0;

  // TODO also have an interface that allows recording / piecewise construction
};

struct SCORE_LIB_PROCESS_EXPORT AutomatableFactoryList : score::InterfaceList<AutomatableFactory>
{
  Process::ProcessModelFactory*
  get(const score::ApplicationContext& ctx, const ossia::complex_type& t) const noexcept
  {
    for (auto& fact : *this)
    {
      if (fact.matches(t))
        return fact.get(ctx);
    }
    return nullptr;
  }
};

}
