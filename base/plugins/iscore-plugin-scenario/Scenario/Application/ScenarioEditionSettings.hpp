#pragma once
#include <Process/ExpandMode.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <QObject>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT EditionSettings : public QObject
{
        Q_OBJECT
        Q_PROPERTY(ExpandMode expandMode READ expandMode WRITE setExpandMode NOTIFY expandModeChanged)
        Q_PROPERTY(Scenario::Tool tool READ tool WRITE setTool NOTIFY toolChanged)
        Q_PROPERTY(bool sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)

        ExpandMode m_expandMode{ExpandMode::Scale};
        Scenario::Tool m_tool{Scenario::Tool::Select};
        bool m_sequence{false};
        bool m_execution{false};

    public:

        ExpandMode expandMode() const;
        Scenario::Tool tool() const;
        bool sequence() const;

        void setExpandMode(ExpandMode expandMode);
        void setTool(Scenario::Tool tool);
        void setSequence(bool sequence);
        void setExecution(bool ex);

        void setDefault();
        void restoreTool();

    signals:
        void expandModeChanged(ExpandMode expandMode);
        void toolChanged(Scenario::Tool tool);
        void sequenceChanged(bool sequence);

    private:
        Scenario::Tool m_previousTool{Scenario::Tool::Select};
};
}
