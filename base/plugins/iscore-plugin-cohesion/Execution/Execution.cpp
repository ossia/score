#include "Execution.hpp"
#include "Execution/ConstraintExecutor.hpp"

Executor::Executor(ConstraintModel& cm)
{
    m_executor = new ConstraintExecutor(cm);
    m_executor->start();
}

void Executor::stop()
{
    m_executor->stop();
}
