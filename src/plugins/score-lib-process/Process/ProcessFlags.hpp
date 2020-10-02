#pragma once
#include <ossia/detail/config.hpp>

#include <cinttypes>
namespace Process
{
class ProcessModel;
class ProcessModelFactory;
class LayerFactory;
class EffectFactory;

/**
 * @brief Various settings for processes.
 */
enum ProcessFlags : int64_t
{
  //! Can be loaded as a process of an interval
  SupportsTemporal = (1 << 0),

  //! Can be loaded in an effect chain
  SupportsEffectChain = (1 << 1),

  //! Can be loaded in a state
  SupportsState = (1 << 2),

  //! Action from the user required upon creation
  RequiresCustomData = (1 << 3),

  //! When created in an interval, go on the top slot or in a new slot
  PutInNewSlot = (1 << 4),

  //! The process won't change if the parent duration changes (eg it's a
  //! filter)
  TimeIndependent = (1 << 5),

  //! The process is snapshottable
  Snapshottable = (1 << 6),

  //! The process is recordable
  Recordable = (1 << 7),

  SupportsLasting = SupportsTemporal | SupportsEffectChain,
  ExternalEffect = SupportsLasting | RequiresCustomData | TimeIndependent,
  SupportsAll = SupportsTemporal | SupportsEffectChain | SupportsState
};

/**
 * \class ProcessFlags_k
 * \brief Metadata to retrieve the ProcessFlags of a process
 */
class ProcessFlags_k;
}
