#include <score/application/ScriptEvaluator.hpp>

namespace score
{
ScriptEvaluator::~ScriptEvaluator() = default;

ScriptEvaluator*& scriptEvaluator() noexcept
{
  static ScriptEvaluator* instance{};
  return instance;
}
}
