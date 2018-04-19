#pragma once
#include <QGridLayout>
#include <QWidget>
#include <memory>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>

namespace score
{
class SelectionDispatcher;
struct DocumentContext;
}

namespace Scenario
{
class TimeSyncModel;

class TimeSyncSummaryWidget final : public QWidget
{
public:
  explicit TimeSyncSummaryWidget(
      const TimeSyncModel&,
      const score::DocumentContext& doc,
      QWidget* parent = nullptr);
  ~TimeSyncSummaryWidget() override;

  const TimeSyncModel& sync;

private:
  score::SelectionDispatcher m_selectionDispatcher;
  score::MarginLess<QGridLayout> m_lay;
};
}
