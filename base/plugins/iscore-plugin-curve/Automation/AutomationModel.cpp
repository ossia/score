#include "AutomationModel.hpp"
#include "AutomationLayerModel.hpp"
#include "State/AutomationState.hpp"

#include "Curve/CurveModel.hpp"
#include "Curve/Segment/Linear/LinearCurveSegmentModel.hpp"
#include "Curve/Point/CurvePointModel.hpp"

#include <iscore/document/DocumentInterface.hpp>

#include "Curve/Segment/PointArray/PointArrayCurveSegmentModel.hpp"
AutomationModel::AutomationModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent) :
    Process {duration, id, processName(), parent},
    m_startState{new AutomationState{*this, 0., this}},
    m_endState{new AutomationState{*this, 1., this}}
{
    pluginModelList = new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this};

    // Named shall be enough ?
    setCurve(new CurveModel{Id<CurveModel>(45345), this});

    auto s1 = new LinearCurveSegmentModel(Id<CurveSegmentModel>(1), m_curve);
    s1->setStart({0., 0.0});
    s1->setEnd({1., 1.});

    m_curve->addSegment(s1);
    connect(m_curve, &CurveModel::changed,
            this, &AutomationModel::curveChanged);

    metadata.setName(QString("Automation.%1").arg(*this->id().val()));
}

AutomationModel::AutomationModel(
        const AutomationModel& source,
        const Id<Process>& id,
        QObject* parent):
    Process{source, id,  processName(), parent},
    m_address(source.address()),
    m_min{source.min()},
    m_max{source.max()},
    m_startState{new AutomationState{*this, 0., this}},
    m_endState{new AutomationState{*this, 1., this}}
{
    setCurve(source.curve().clone(source.curve().id(), this));
    pluginModelList = new iscore::ElementPluginModelList(*source.pluginModelList, this);
    connect(m_curve, &CurveModel::changed,
            this, &AutomationModel::curveChanged);
    metadata.setName(QString("Automation.%1").arg(*this->id().val()));
}

Process* AutomationModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new AutomationModel {*this, newId, newParent};
}

QString AutomationModel::processName() const
{
    return "Automation";
}

void AutomationModel::setDurationAndScale(const TimeValue& newDuration)
{
    // We only need to change the duration.
    setDuration(newDuration);
    m_curve->changed();
}

void AutomationModel::setDurationAndGrow(const TimeValue& newDuration)
{
    // If there are no segments, nothing changes
    if(m_curve->segments().size() == 0)
    {
        setDuration(newDuration);
        return;
    }

    // Else, scale all the segments by the increase.
    double scale = duration() / newDuration;
    for(auto& segment : m_curve->segments())
    {
        CurvePoint pt = segment.start();
        pt.setX(pt.x() * scale);
        segment.setStart(pt);

        pt = segment.end();
        pt.setX(pt.x() * scale);
        segment.setEnd(pt);
    }

    setDuration(newDuration);
    m_curve->changed();
}

void AutomationModel::setDurationAndShrink(const TimeValue& newDuration)
{
    // If there are no segments, nothing changes
    if(m_curve->segments().size() == 0)
    {
        setDuration(newDuration);
        return;
    }

    // Else, scale all the segments by the increase.
    double scale = duration() / newDuration;
    for(auto& segment : m_curve->segments())
    {
        CurvePoint pt = segment.start();
        pt.setX(pt.x() * scale);
        segment.setStart(pt);

        pt = segment.end();
        pt.setX(pt.x() * scale);
        segment.setEnd(pt);
    }

    // Since we shrink, scale > 1. so we have to cut.
    // Note:  this will certainly change how some functions do look.
    auto segments = m_curve->segments(); // Make a copy since we will change the map.
    for(auto& segment : segments)
    {
        if(segment.start().x() >= 1.)
        {
            // bye
            m_curve->removeSegment(&segment);
        }
        else if(segment.end().x() >= 1.)
        {
            auto end = segment.end();
            end.setX(1.);
            segment.setEnd(end);
        }
    }

    setDuration(newDuration);
    m_curve->changed();
}

void AutomationModel::startExecution()
{
    emit execution(true);
}

void AutomationModel::stopExecution()
{
    emit execution(false);
}

void AutomationModel::reset()
{

}

Selection AutomationModel::selectableChildren() const
{
    Selection s;
    for(auto& segment : m_curve->segments())
        s.insert(&segment);
    for(auto& point : m_curve->points())
        s.insert(point);
    return s;
}

Selection AutomationModel::selectedChildren() const
{
    return m_curve->selectedChildren();
}

void AutomationModel::setSelection(const Selection & s) const
{
    m_curve->setSelection(s);
}

LayerModel* AutomationModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto vm = new AutomationLayerModel{*this, viewModelId, parent};
    return vm;
}

LayerModel* AutomationModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto vm = new AutomationLayerModel {
              static_cast<const AutomationLayerModel&>(source), *this, newId, parent};
    return vm;
}

void AutomationModel::setCurve(CurveModel* newCurve)
{
    delete m_curve;
    m_curve = newCurve;

    connect(m_curve, &CurveModel::changed,
            this, [&] () {
        emit curveChanged();

        m_startState->messagesChanged(m_startState->messages());
        m_endState->messagesChanged(m_endState->messages());
    });

    emit m_curve->changed();
}


AutomationState* AutomationModel::startState() const
{
    return m_startState;
}

AutomationState* AutomationModel::endState() const
{
    return m_endState;
}

iscore::Address AutomationModel::address() const
{
    return m_address;
}

double AutomationModel::value(const TimeValue& time)
{
    return -1;
}

double AutomationModel::min() const
{
    return m_min;
}

double AutomationModel::max() const
{
    return m_max;
}

void AutomationModel::setAddress(const iscore::Address &arg)
{
    if(m_address == arg)
    {
        return;
    }

    m_address = arg;
    emit addressChanged(arg);
    emit m_curve->changed();
}

void AutomationModel::setMin(double arg)
{
    if (m_min == arg)
        return;

    m_min = arg;
    emit minChanged(arg);
    emit m_curve->changed();
}

void AutomationModel::setMax(double arg)
{
    if (m_max == arg)
        return;

    m_max = arg;
    emit maxChanged(arg);
    emit m_curve->changed();
}

