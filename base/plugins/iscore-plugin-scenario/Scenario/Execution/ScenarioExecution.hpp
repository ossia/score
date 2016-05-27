#pragma once
#include <QObject>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
class ScenarioInterface;
class ScenarioModel;
class StateModel;
struct ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioExecution : public QObject
{
        Q_OBJECT
    signals:
        void playState(const Scenario::ScenarioInterface&, Id<Scenario::StateModel>) const;
        void playAtDate(const TimeValue&) const;

        void startRecording(const Scenario::ScenarioModel&, Scenario::Point) const;
        void startRecordingMessages(const Scenario::ScenarioModel&, Scenario::Point) const;
        void stopRecording() const;
};
}
