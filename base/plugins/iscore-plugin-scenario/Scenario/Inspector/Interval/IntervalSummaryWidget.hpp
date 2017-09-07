#pragma once
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <QWidget>
#include <QGridLayout>

namespace iscore
{
class SelectionDispatcher;
struct DocumentContext;
}

namespace Scenario
{
class IntervalModel;
class IntervalSummaryWidget : public QWidget
{
public:
  explicit IntervalSummaryWidget(
      const IntervalModel& object, const iscore::DocumentContext& doc,
      QWidget* parent = nullptr);

private:
  iscore::SelectionDispatcher m_selectionDispatcher;
  iscore::MarginLess<QGridLayout> m_lay;
};
}
