#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Scenario/Application/Drops/DropLayerInScenario.hpp>
#include <Scenario/Application/Drops/DropPresetInInterval.hpp>
#include <Scenario/Application/Drops/DropPresetInScenario.hpp>
#include <Scenario/Application/Drops/DropProcessInInterval.hpp>
#include <Scenario/Application/Drops/DropProcessInScenario.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

#include <QSet>

namespace score
{
struct DocumentContext;
}
namespace Scenario
{
namespace Command
{
class Macro;
}

class ProcessDataDropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("c2829c8c-e1e7-4f90-b67a-d75d77d297f2")
public:
  QSet<QString> mimeTypes() const noexcept override
  {
    return {score::mime::processdata()};
  }

  void dropCustom(
      std::vector<ProcessDrop>& vec, const QMimeData& mime,
      const score::DocumentContext& ctx) const noexcept override
  try
  {
    Mime<Process::ProcessData>::Deserializer des{mime};

    ProcessDrop p;
    p.creation = des.deserialize();
    vec.push_back(p);
  }
  catch(...)
  {
  }
};

class DropScenario final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("34961e8b-19a5-408f-af90-55f59ce8c58a")

public:
  DropScenario();

private:
  bool
  drop(const Scenario::ScenarioPresenter&, QPointF pos, const QMimeData& mime) override;
};

class DropScoreInScenario final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("63fc2b70-79b2-4bf8-a1f6-c148b8eceba8")
public:
  DropScoreInScenario();

private:
  bool
  drop(const Scenario::ScenarioPresenter&, QPointF pos, const QMimeData& mime) override;
};

/**
 * @brief What happens when a .score file is dropped in an interval.
 */
class DropScoreInInterval final : public IntervalDropHandler
{
  SCORE_CONCRETE("46cb9918-fe25-4123-ab61-68ce3939b80a")

  bool drop(
      const score::DocumentContext& ctx, const Scenario::IntervalModel&, QPointF p,
      const QMimeData& mime) override;
};

/**
 * @brief The AutomationDropHandler class
 * Will create an automation where the addresses are dropped.
 */
class AutomationDropHandler final : public IntervalDropHandler
{
  SCORE_CONCRETE("851c98e1-4bcb-407b-9a72-8288d83c9f38")

  bool drop(
      const score::DocumentContext& ctx, const Scenario::IntervalModel&, QPointF p,
      const QMimeData& mime) override;
};
}
