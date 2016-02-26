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
class TimeNodeModel;

class TimeNodeSummaryWidget : public QWidget
{
    public:
	explicit TimeNodeSummaryWidget(const TimeNodeModel&, const iscore::DocumentContext& doc, QWidget *parent = 0);

    private:
	std::unique_ptr<iscore::SelectionDispatcher> m_selectionDispatcher;

};

}
