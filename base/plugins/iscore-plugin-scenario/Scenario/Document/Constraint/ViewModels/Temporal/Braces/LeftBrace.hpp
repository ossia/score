#pragma once

// RENAMEME ConstraintBraces

#include "ConstraintBrace.hpp"
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
namespace Scenario
{

class ISCORE_PLUGIN_SCENARIO_EXPORT LeftBraceView final
    : public ConstraintBrace
{
public:
  LeftBraceView(const ConstraintView& parentCstr, QQuickPaintedItem* parent)
      : ConstraintBrace{parentCstr, parent}
  {
  }

  static constexpr int static_type()
  {
    return 1337 + ItemType::LeftBrace;
  }
  int type() const override
  {
    return static_type();
  }
};

class ISCORE_PLUGIN_SCENARIO_EXPORT RightBraceView final
    : public ConstraintBrace
{
public:
  RightBraceView(const ConstraintView& parentCstr, QQuickPaintedItem* parent)
      : ConstraintBrace{parentCstr, parent}
  {
    this->setRotation(180);
  }

  static constexpr int static_type()
  {
    return 1337 + ItemType::RightBrace;
  }
  int type() const override
  {
    return static_type();
  }
};
}
