#pragma once

namespace Scenario
{
namespace Command
{
class AddLayerModelToSlot;
}
}
class AddOnlyProcessToConstraint;

class CurveModel;
class PointArrayCurveSegmentModel;
namespace Scenario { class ScenarioModel; }
class DeviceExplorerModel;

struct RecordData
{
        RecordData(
                AddOnlyProcessToConstraint* cmd_proc,
                Scenario::Command::AddLayerModelToSlot* cmd_lay,
                CurveModel& cm,
                PointArrayCurveSegmentModel& seg):
            addProcCmd{cmd_proc},
            addLayCmd{cmd_lay},
            curveModel{cm},
            segment{seg}
        { }

        AddOnlyProcessToConstraint* addProcCmd{};
        Scenario::Command::AddLayerModelToSlot* addLayCmd{};

        CurveModel& curveModel;
        PointArrayCurveSegmentModel& segment;
};
