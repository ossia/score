#pragma once
#include <QString>

#include <score_lib_base_export.h>

namespace score
{
struct DocumentContext;

/**
 * @brief Running a script on this machine, for somebody who is not here.
 *
 * A terminal's console edits a score that runs elsewhere. Commands happen to
 * replicate, so `Score.createProcess` appears to work -- but everything that is
 * not a command runs against the terminal's own document, where there are no
 * devices, no execution and no hardware. `Score.device("x")` is null there and
 * always will be.
 *
 * Rather than forwarding one call at a time, the script itself goes to the
 * machine that can answer it. Which means the session layer has to run
 * JavaScript, and it has no business knowing what JavaScript is: it looks up
 * this interface, which the JS plug-in registers if it is loaded, and finds
 * nothing if it is not.
 */
struct SCORE_LIB_BASE_EXPORT ScriptEvaluator
{
  virtual ~ScriptEvaluator();

  //! Evaluate `code` against `ctx`. The returned string is what a console
  //! would have printed -- the value, or the error.
  virtual QString evaluate(const score::DocumentContext& ctx, const QString& code) = 0;
};

//! The evaluator for this process, or null when nothing registered one.
//! Set once at startup by whichever plug-in can actually run scripts.
SCORE_LIB_BASE_EXPORT ScriptEvaluator*& scriptEvaluator() noexcept;
}
