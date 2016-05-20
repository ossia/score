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
class EventModel;
class EventSummaryWidget final : public QWidget
{
    public:
    EventSummaryWidget( const EventModel& object, const iscore::DocumentContext& doc, QWidget* parent = nullptr);

    private:
        std::unique_ptr<iscore::SelectionDispatcher> m_selectionDispatcher;

};
}
