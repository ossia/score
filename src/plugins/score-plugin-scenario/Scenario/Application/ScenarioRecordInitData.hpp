#pragma once
#include <score/tools/Debug.hpp>

#include <QDataStream>
#include <QPointF>

#include <verdigris>

namespace Process
{
class LayerPresenter;
}

namespace Scenario
{
struct ScenarioRecordInitData
{
  ScenarioRecordInitData() { }
  ScenarioRecordInitData(const Process::LayerPresenter* lp, QPointF p)
      : presenter{lp}
      , point{p}
  {
  }

  const Process::LayerPresenter* presenter{};
  QPointF point;
};
}

inline QDataStream&
operator<<(QDataStream& i, const Scenario::ScenarioRecordInitData& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator>>(QDataStream& i, Scenario::ScenarioRecordInitData& sel)
{
  SCORE_ABORT;
  return i;
}
Q_DECLARE_METATYPE(Scenario::ScenarioRecordInitData)
W_REGISTER_ARGTYPE(Scenario::ScenarioRecordInitData)
