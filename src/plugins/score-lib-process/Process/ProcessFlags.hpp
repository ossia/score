#pragma once
#include <ossia/detail/config.hpp>

#include <cinttypes>
namespace Process
{
class ProcessModel;
class ProcessModelFactory;
class LayerFactory;
class EffectFactory;

#define SCORE_FLAG(i) (1 << i)
/**
 * @brief Various settings for processes.
 */
enum ProcessFlags : int64_t
{
  //! Can be loaded as a process of an interval
  SupportsTemporal =   SCORE_FLAG(0),

  //! The process won't change if the parent duration changes (eg it's a filter)
  TimeIndependent =    SCORE_FLAG(1),

  //! Can be loaded in a state
  SupportsState =      SCORE_FLAG(2),

  //! Action from the user required upon creation
  RequiresCustomData = SCORE_FLAG(3),

  //! When created in an interval, go on the top slot or in a new slot
  PutInNewSlot =       SCORE_FLAG(4),

  //! The presenter / view already handles rendering when the model loops.
  HandlesLooping =     SCORE_FLAG(5),

  //! The process supports being exposed to the ControlSurface
  ControlSurface =     SCORE_FLAG(6),

  SupportsLasting = SupportsTemporal | TimeIndependent,
  ExternalEffect = SupportsTemporal | TimeIndependent | RequiresCustomData | ControlSurface,
  SupportsAll = SupportsTemporal | TimeIndependent | SupportsState
};

/**
 * \class ProcessFlags_k
 * \brief Metadata to retrieve the ProcessFlags of a process
 */
class ProcessFlags_k;
}
