#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "State/AutomationState.hpp"

#include "CurveTest/CurveModel.hpp"
#include "CurveTest/LinearCurveSegmentModel.hpp"

AutomationModel::AutomationModel(
        const TimeValue& duration,
        const id_type<ProcessModel>& id,
        QObject* parent) :
    ProcessModel {duration, id, processName(), parent}
{
    // Named shall be enough ?
    m_curve = new CurveModel{id_type<CurveModel>(45345), this};

    auto s1 = new LinearCurveSegmentModel(id_type<CurveSegmentModel>(1), m_curve);
    s1->setStart({0., 0.0});
    s1->setEnd({0.2, 1.});

    auto s2 = new GammaCurveSegmentModel(id_type<CurveSegmentModel>(2), m_curve);
    s2->setStart({0.2, 0.9});
    s2->setEnd({0.4, 0.5});
    s2->setPrevious(s1->id());
    s1->setFollowing(s2->id());

    auto s3 = new GammaCurveSegmentModel(id_type<CurveSegmentModel>(3), m_curve);
    s3->setStart({0.4, 0.5});
    s3->setEnd({0.6, 1.0});
    s3->setPrevious(s2->id());
    s2->setFollowing(s3->id());
    s3->gamma = 10;


    auto s4 = new SinCurveSegmentModel(id_type<CurveSegmentModel>(4), m_curve);
    s4->setStart({0.7, 0.0});
    s4->setEnd({1.0, 1.});

    m_curve->addSegment(s1);
    m_curve->addSegment(s2);
    m_curve->addSegment(s3);
    m_curve->addSegment(s4);
}

ProcessModel* AutomationModel::clone(
        const id_type<ProcessModel>& newId,
        QObject* newParent)
{
    auto autom = new AutomationModel {this->duration(), newId, newParent};
    autom->setAddress(address());
    autom->setPoints(QMap<double, double> {points() });

    return autom;
}

QString AutomationModel::processName() const
{
    return "Automation";
}

void AutomationModel::setDurationAndScale(const TimeValue& newDuration)
{
}

void AutomationModel::setDurationAndGrow(const TimeValue& newDuration)
{
}

void AutomationModel::setDurationAndShrink(const TimeValue& newDuration)
{
}

Selection AutomationModel::selectableChildren() const
{
    return {};
}

Selection AutomationModel::selectedChildren() const
{
    return m_curve->selectedChildren();
}

void AutomationModel::setSelection(const Selection & s) const
{
    m_curve->setSelection(s);
}

ProcessViewModel* AutomationModel::makeViewModel_impl(
        const id_type<ProcessViewModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto vm = new AutomationViewModel{*this, viewModelId, parent};
    return vm;
}

ProcessViewModel* AutomationModel::cloneViewModel_impl(
        const id_type<ProcessViewModel>& newId,
        const ProcessViewModel& source,
        QObject* parent)
{
    auto vm = new AutomationViewModel {
              static_cast<const AutomationViewModel&>(source), *this, newId, parent};
    return vm;
}


// TODO fix memory leak
ProcessStateDataInterface* AutomationModel::startState() const
{
    return new AutomationState{this, 0.};
}

ProcessStateDataInterface* AutomationModel::endState() const
{
    return new AutomationState{this, 1.};
}

QString AutomationModel::address() const
{
    return m_address;
}

const QMap<double, double> &AutomationModel::points() const
{
    return m_points;
}

void AutomationModel::setPoints(QMap<double, double> &&points)
{
    m_points = std::move(points);
}

// Note : the presenter should see the modifications happening,
// and prevent accidental point removal.
void AutomationModel::addPoint(double x, double y)
{
    m_points[x] = y;
    emit pointsChanged();
}

void AutomationModel::removePoint(double x)
{
    m_points.remove(x);
    emit pointsChanged();
}

void AutomationModel::movePoint(double oldx, double newx, double newy)
{
    m_points.remove(oldx);

    m_points[newx] = newy;
    emit pointsChanged();
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

void AutomationModel::setAddress(const QString &arg)
{
    if(m_address == arg)
    {
        return;
    }

    m_address = arg;
    emit addressChanged(arg);
}

void AutomationModel::setMin(double arg)
{
    if (m_min == arg)
        return;

    m_min = arg;
    emit minChanged(arg);
}

void AutomationModel::setMax(double arg)
{
    if (m_max == arg)
        return;

    m_max = arg;
    emit maxChanged(arg);
}
