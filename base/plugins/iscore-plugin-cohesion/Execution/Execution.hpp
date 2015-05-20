#pragma once
class ConstraintModel;
class ConstraintExecutor;

class Executor
{
  public:
    Executor(ConstraintModel& cm);
    void stop();

    private:
    ConstraintExecutor* m_executor{};
};
