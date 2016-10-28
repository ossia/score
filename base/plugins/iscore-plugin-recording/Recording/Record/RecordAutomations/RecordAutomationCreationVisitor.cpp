#include "RecordAutomationCreationVisitor.hpp"
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Automation/AutomationModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>

namespace Recording
{

RecordData RecordAutomationCreationVisitor::makeCurve(float start_y)
{
    // Note : since we directly create the IDs here, we don't have to worry
    // about their generation.
    auto cmd_proc = new Scenario::Command::AddOnlyProcessToConstraint{
            Path<Scenario::ConstraintModel>(box.constraint),
            Metadata<ConcreteFactoryKey_k, Automation::ProcessModel>::get()};
    cmd_proc->redo();
    auto& proc = box.constraint.processes.at(cmd_proc->processId());
    auto& autom = static_cast<Automation::ProcessModel&>(proc);


    auto cmd_layer = new Scenario::Command::AddLayerModelToSlot{
            box.slot, proc};
    cmd_layer->redo();

    autom.curve().clear();

    // TODO handle other domain types for vec.
    auto min = addr.domain.convert_min<float>();
    auto max = addr.domain.convert_max<float>();

    Curve::SegmentData seg;
    seg.id = Id<Curve::SegmentModel>{0};
    seg.start = {0, start_y};
    seg.end = {1, -1};
    seg.specificSegmentData =
            QVariant::fromValue(
                Curve::PointArraySegmentData{ 0, 1, min, max, { {0, start_y} } });
    auto segt = new Curve::PointArraySegment{
            seg,
            &autom.curve()};

    segt->setStart({0, start_y});
    segt->setEnd({1, -1});
    segt->addPoint(0, start_y);

    autom.curve().addSegment(segt);
    return RecordData{
        cmd_proc,
                cmd_layer,
                autom.curve(),
                *segt};
}

void RecordAutomationCreationVisitor::handle_numeric(float val)
{
    addresses.back().push_back(Device::address(node).address);
    recorder.numeric_records.insert(
                std::make_pair(
                    addresses.back().back(),
                    makeCurve(val)));
}

void RecordAutomationCreationVisitor::operator()(std::array<float, 2> val)
{
    // here we create one curve per component.

    // The address is added only once
    addresses.back().push_back(Device::address(node).address);
    recorder.vec2_records.insert(
                std::make_pair(
                    addresses.back().back(),
                    std::array<RecordData, 2>{makeCurve(val[0]), makeCurve(val[1])}));
}

void RecordAutomationCreationVisitor::operator()(std::array<float, 3> val)
{
    // here we create one curve per component.

    // The address is added only once
    addresses.back().push_back(Device::address(node).address);
    recorder.vec3_records.insert(
                std::make_pair(
                    addresses.back().back(),
                    std::array<RecordData, 3>{makeCurve(val[0]), makeCurve(val[1]), makeCurve(val[2])}));
}

void RecordAutomationCreationVisitor::operator()(std::array<float, 4> val)
{
    // here we create one curve per component.

    // The address is added only once
    addresses.back().push_back(Device::address(node).address);
    recorder.vec4_records.insert(
                std::make_pair(
                    addresses.back().back(),
                    std::array<RecordData, 4>{makeCurve(val[0]), makeCurve(val[1]), makeCurve(val[2]), makeCurve(val[3])}));
}

void RecordAutomationCreationVisitor::operator()(float f) { handle_numeric(f); }

void RecordAutomationCreationVisitor::operator()(int f) { handle_numeric(f); }

void RecordAutomationCreationVisitor::operator()(char f) { handle_numeric(f); }

void RecordAutomationCreationVisitor::operator()(bool f) { handle_numeric(f); }

}
