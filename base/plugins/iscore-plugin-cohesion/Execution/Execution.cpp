#include "Execution.hpp"
#include "Execution/ConstraintExecutor.hpp"

Executor::Executor(ConstraintModel& cm)
{
    auto executor = new ConstraintExecutor(cm);
    executor->start();
}
