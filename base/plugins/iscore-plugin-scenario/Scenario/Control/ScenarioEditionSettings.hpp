#pragma once
#include <QObject>
#include <Scenario/Process/Temporal/StateMachines/Tool.hpp>
#include <Process/ExpandMode.hpp>

class ScenarioEditionSettings : public QObject
{
        Q_OBJECT
        Q_PROPERTY(ExpandMode expandMode READ expandMode WRITE setExpandMode NOTIFY expandModeChanged)
        Q_PROPERTY(ScenarioToolKind tool READ tool WRITE setTool NOTIFY toolChanged)
        Q_PROPERTY(bool sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)

        ExpandMode m_expandMode{ExpandMode::Scale};
        ScenarioToolKind m_tool{ScenarioToolKind::Select};
        bool m_sequence{false};

    public:
        ExpandMode expandMode() const;
        ScenarioToolKind tool() const;
        bool sequence() const;

        void setExpandMode(ExpandMode expandMode);
        void setTool(ScenarioToolKind tool);
        void setSequence(bool sequence);

    signals:
        void expandModeChanged(ExpandMode expandMode);
        void toolChanged(ScenarioToolKind tool);
        void sequenceChanged(bool sequence);
};
