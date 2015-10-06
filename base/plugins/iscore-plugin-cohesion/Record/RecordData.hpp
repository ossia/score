#pragma once

namespace Scenario
{
namespace Command
{
class AddProcessToConstraint;
}
}
class CurveModel;
class PointArrayCurveSegmentModel;
class ScenarioModel;
class DeviceExplorerModel;

struct RecordData
{
        RecordData(
                Scenario::Command::AddProcessToConstraint* cmd,
                CurveModel& cm,
                PointArrayCurveSegmentModel& seg):
            addProcCmd{cmd},
            curveModel{cm},
            segment{seg}
        { }

        Scenario::Command::AddProcessToConstraint* addProcCmd{};

        CurveModel& curveModel;
        PointArrayCurveSegmentModel& segment;
};
