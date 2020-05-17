#pragma once
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
/**
 * @brief The MessageDropHandler class
 * Will create a state in the scenario at the
 * point a MessageList is dropped.
 */
class MessageDropHandler final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("75156fa7-e083-4c9c-a88c-3a05c54f330f")

public:
  MessageDropHandler();

private:
  bool drop(const ScenarioPresenter&, QPointF drop, const QMimeData& mime) override;
};
}
