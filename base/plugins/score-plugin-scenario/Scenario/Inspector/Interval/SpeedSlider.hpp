#pragma once
#include <QWidget>
namespace score
{
struct DocumentContext;
}

namespace Scenario
{
class IntervalModel;
class SpeedSlider final
    : public QWidget
{
public:
  SpeedSlider(
      const IntervalModel& model
      , const score::DocumentContext&
      , bool withButtons
      , QWidget* parent);

private:
  const IntervalModel& m_model;
};
}
