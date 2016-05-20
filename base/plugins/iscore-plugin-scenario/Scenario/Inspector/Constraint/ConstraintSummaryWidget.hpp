#pragma once
#include <QWidget>

#include <memory>

namespace iscore
{
class SelectionDispatcher;
struct DocumentContext;
}

namespace Scenario
{
class ConstraintModel;
class ConstraintSummaryWidget : public QWidget
{
    public:
    explicit ConstraintSummaryWidget(const ConstraintModel& object, const iscore::DocumentContext& doc, QWidget *parent = nullptr);

    private:
    std::unique_ptr<iscore::SelectionDispatcher> m_selectionDispatcher;
};
}
