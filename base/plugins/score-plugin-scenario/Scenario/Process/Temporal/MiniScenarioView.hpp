#pragma once
#include <Process/LayerView.hpp>
#include <Process/TimeValue.hpp>

#include <nano_observer.hpp>

namespace Scenario
{
class ProcessModel;
class MiniScenarioView final
    : public QObject
    , public Process::MiniLayer
    , public Nano::Observer
{
public:
    MiniScenarioView(const Scenario::ProcessModel& sc, QGraphicsItem* p);

private:
    template<typename... T>
    void on_elementChanged(T&&...)
    {
      update();
    }

    void paint_impl(QPainter*) const override;
    const Scenario::ProcessModel& m_scenario;
};
}
