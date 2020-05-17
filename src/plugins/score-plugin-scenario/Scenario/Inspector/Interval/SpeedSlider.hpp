#pragma once
#include <QPointer>
#include <QWidget>

#include <score_plugin_scenario_export.h>
namespace score
{
struct DocumentContext;
struct SpeedSlider;
}

namespace Scenario
{
class IntervalModel;
class SCORE_PLUGIN_SCENARIO_EXPORT SpeedWidget final : public QWidget
{
public:
  SpeedWidget(bool withButtons, bool showText, QWidget* parent);
  ~SpeedWidget() override;

  void setInterval(const IntervalModel&);
  void unsetInterval();

private:
  QSize sizeHint() const override;
  QPointer<const IntervalModel> m_model;
  score::SpeedSlider* m_slider{};
};
}
