#pragma once
#include <QWidget>
#include <score/widgets/MarginLess.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <QGridLayout>

#include <memory>

namespace score
{
class SelectionDispatcher;
struct DocumentContext;
}

namespace Scenario
{
class TimeSyncModel;

class TimeSyncSummaryWidget : public QWidget
{
public:
  explicit TimeSyncSummaryWidget(
      const TimeSyncModel&, const score::DocumentContext& doc,
      QWidget* parent = nullptr);

private:
  score::SelectionDispatcher m_selectionDispatcher;
  score::MarginLess<QGridLayout> m_lay;
};
}
