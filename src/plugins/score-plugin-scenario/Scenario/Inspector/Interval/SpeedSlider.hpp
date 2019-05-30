#pragma once
#include <QWidget>

#include <score_plugin_scenario_export.h>
namespace score
{
struct DocumentContext;
}

namespace Scenario
{
class IntervalModel;
class SCORE_PLUGIN_SCENARIO_EXPORT SpeedWidget final : public QWidget
{
public:
  SpeedWidget(
      const IntervalModel& model,
      const score::DocumentContext&,
      bool withButtons,
      bool showText,
      QWidget* parent);
  ~SpeedWidget() override;

private:
  QSize sizeHint() const override;
  const IntervalModel& m_model;
};
}
