#pragma once

namespace Scenario
{
class ScenarioModel;
namespace Command
{
class AddLayerModelToSlot;
class AddOnlyProcessToConstraint;
}
}
namespace Curve
{
class Model;
class PointArraySegment;
}
namespace DeviceExplorer
{
class DeviceExplorerModel;
}
struct RecordData
{
        RecordData(
                Scenario::Command::AddOnlyProcessToConstraint* cmd_proc,
                Scenario::Command::AddLayerModelToSlot* cmd_lay,
                Curve::Model& cm,
                Curve::PointArraySegment& seg):
            addProcCmd{cmd_proc},
            addLayCmd{cmd_lay},
            curveModel{cm},
            segment{seg}
        { }

        Scenario::Command::AddOnlyProcessToConstraint* addProcCmd{};
        Scenario::Command::AddLayerModelToSlot* addLayCmd{};

        Curve::Model& curveModel;
        Curve::PointArraySegment& segment;
};
