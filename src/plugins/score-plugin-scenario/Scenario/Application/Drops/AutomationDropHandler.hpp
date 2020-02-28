#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
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

  std::vector<ProcessDrop>
  drop(const QMimeData& mime, const score::DocumentContext& ctx) const
      noexcept override
  try
  {
    Mime<Process::ProcessData>::Deserializer des{mime};

    ProcessDrop p;
    p.creation = des.deserialize();
    return {p};
  }
  catch(...)
  {
    return {};
  }
};

class DropProcessInScenario final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("9a094988-b05f-4e10-8e0d-56e8d46e084d")

public:
  DropProcessInScenario();
  void init();
private:
  bool drop(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime) override;
};

class DropLayerInScenario final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("0eb96d95-3f5f-4e7a-b806-d03d0ac88b48")

public:
  DropLayerInScenario();
private:
  bool drop(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime) override;
};

class DropScenario final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("34961e8b-19a5-408f-af90-55f59ce8c58a")

public:
  DropScenario();
private:
  bool drop(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime) override;
};

class DropScore final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("63fc2b70-79b2-4bf8-a1f6-c148b8eceba8")
public:
  DropScore();

private:
  bool drop(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime) override;
};
class DropLayerInInterval final : public IntervalDropHandler
{
  SCORE_CONCRETE("9df2eac6-6680-43cc-9634-60324416ba04")

  bool drop(
      const score::DocumentContext& ctx,
      const Scenario::IntervalModel&,
      QPointF p,
      const QMimeData& mime) override;

public:
  static void perform(
      const IntervalModel& interval,
      const score::DocumentContext& doc,
      Scenario::Command::Macro& m,
      const QJsonObject& json);
};
/**
 * @brief The ProcessDropHandler class
 * Will create a blank process.
 */
class DropProcessInInterval final : public IntervalDropHandler
{
  SCORE_CONCRETE("08f5aec5-3a42-45c8-b3db-aa45a851dd09")

  bool drop(
      const score::DocumentContext& ctx,
      const Scenario::IntervalModel&,
      QPointF p,
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
      const score::DocumentContext& ctx,
      const Scenario::IntervalModel&,
      QPointF p,
      const QMimeData& mime) override;
};
}
