#pragma once

// RENAMEME IntervalBraces

#include "IntervalBrace.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
namespace Scenario
{

class SCORE_PLUGIN_SCENARIO_EXPORT LeftBraceView final : public IntervalBrace
{
public:
  LeftBraceView(const IntervalView& parentCstr, QGraphicsItem* parent)
      : IntervalBrace{parentCstr, parent}
  {
  }
  ~LeftBraceView() override;

  static const constexpr int Type = ItemType::LeftBrace;
  int type() const final override { return Type; }
};

class SCORE_PLUGIN_SCENARIO_EXPORT RightBraceView final : public IntervalBrace
{
public:
  RightBraceView(const IntervalView& parentCstr, QGraphicsItem* parent)
      : IntervalBrace{parentCstr, parent}
  {
    this->setRotation(180);
  }
  ~RightBraceView() override;

  static const constexpr int Type = ItemType::RightBrace;
  int type() const final override { return Type; }
};
}
