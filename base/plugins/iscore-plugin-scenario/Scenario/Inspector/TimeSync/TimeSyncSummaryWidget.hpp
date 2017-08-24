#pragma once
#include <QWidget>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <QGridLayout>

#include <memory>

namespace iscore
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
      const TimeSyncModel&, const iscore::DocumentContext& doc,
      QWidget* parent = nullptr);

private:
  iscore::SelectionDispatcher m_selectionDispatcher;
  iscore::MarginLess<QGridLayout> m_lay;
};
}
