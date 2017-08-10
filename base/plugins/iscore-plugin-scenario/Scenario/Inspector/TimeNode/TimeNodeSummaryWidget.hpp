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
class TimeNodeModel;

class TimeNodeSummaryWidget : public QWidget
{
public:
  explicit TimeNodeSummaryWidget(
      const TimeNodeModel&, const iscore::DocumentContext& doc,
      QWidget* parent = nullptr);

private:
  iscore::SelectionDispatcher m_selectionDispatcher;
  iscore::MarginLess<QGridLayout> m_lay;
};
}
