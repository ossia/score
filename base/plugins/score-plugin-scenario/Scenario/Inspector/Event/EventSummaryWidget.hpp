#pragma once

#include <QWidget>
#include <memory>

namespace score
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
  EventSummaryWidget(
      const EventModel& object, const score::DocumentContext& doc,
      QWidget* parent = nullptr);
  ~EventSummaryWidget() override;
const EventModel& event;
private:
  std::unique_ptr<score::SelectionDispatcher> m_selectionDispatcher;
};
}
