#pragma once
#include <QObject>
#include <Scenario/Process/Temporal/StateMachines/Tool.hpp>
#include <Process/ExpandMode.hpp>

namespace Scenario
{
class EditionSettings : public QObject
{
        Q_OBJECT
        Q_PROPERTY(ExpandMode expandMode READ expandMode WRITE setExpandMode NOTIFY expandModeChanged)
        Q_PROPERTY(Scenario::Tool tool READ tool WRITE setTool NOTIFY toolChanged)
        Q_PROPERTY(bool sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)

        ExpandMode m_expandMode{ExpandMode::Scale};
        Scenario::Tool m_tool{Scenario::Tool::Select};
        bool m_sequence{false};

    public:
        ExpandMode expandMode() const;
        Scenario::Tool tool() const;
        bool sequence() const;

        void setExpandMode(ExpandMode expandMode);
        void setTool(Scenario::Tool tool);
        void setSequence(bool sequence);

    signals:
        void expandModeChanged(ExpandMode expandMode);
        void toolChanged(Scenario::Tool tool);
        void sequenceChanged(bool sequence);
};
}
