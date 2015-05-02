#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "State/AutomationState.hpp"

AutomationModel::AutomationModel(
        const TimeValue& duration,
        const id_type<ProcessSharedModelInterface>& id,
        QObject* parent) :
    ProcessSharedModelInterface {duration, id, processName(), parent}
{

    // Demo
    addPoint(0, 0);
    addPoint(0.5, 0.3);
    addPoint(1, 1);
}

ProcessSharedModelInterface* AutomationModel::clone(
        const id_type<ProcessSharedModelInterface>& newId,
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
    // Due to the way the plug-in is done, we have to think backwards :
    // The data is between 0 and 1, and the duration sets the time it takes
    // to go from x=0 to x=1.
    // Hence we just have to change the duration here.

    setDuration(newDuration);
    emit pointsChanged();

    // Note : the presenter must draw the points correctly, by taking
    // into account the seconds <-> pixel ratio, and the zoom.
}

void AutomationModel::setDurationAndGrow(const TimeValue& newDuration)
{
    double scale = duration() / newDuration;

    // The duration can only grow from the outside (constraint scaling), not shrink
    if(scale < 1)
    {
        auto points = m_points;
        auto keys = m_points.keys();
        m_points.clear();

        // 1. Scale the x axis by multiplying each x value.
        for(int i = 0; i < keys.size(); ++i)
        {
            m_points[keys[i] * scale] = points[keys[i]];
        }

        if(m_points.keys().size() >= 2)
        {
            // 2. Create a new point at the end (x=1) with the same value than the last point.
            // If there is already another point at the same "value" in-between, we optimize
            // NOTE : One day this may be source of headache.
            auto newkeys = m_points.keys();

            double butlastKey = newkeys.at(newkeys.size() - 2);
            double lastKey = newkeys.at(newkeys.size() - 1);

            if(m_points[butlastKey] == m_points[lastKey])
            {
                auto val = m_points.take(lastKey);
                m_points[1] = val;
            }
            else
            {
                auto last = m_points.last();
                m_points[1] = last;
            }
        }
        setDuration(newDuration);
        emit pointsChanged();
    }
}

void AutomationModel::setDurationAndShrink(const TimeValue& newDuration)
{
    double scale = duration() / newDuration;
    if(scale > 1)
    {
        auto points = m_points;
        auto keys = m_points.keys();
        m_points.clear();

        // Scale all the points
        for(int i = 0; i < keys.size(); ++i)
        {
            double newKey = keys[i] * scale;
            m_points[newKey] = points[keys[i]];

            // Find the first point where the key is > 1
            if(newKey == 1.)
            {
                break;
            }
            else if(newKey > 1.)
            {
                double x1 = keys[i-1] * scale;
                double x2 = newKey;
                double y1 = m_points[x1];
                double y2 = m_points[x2];

                m_points[1.] = (y2 - y1 + x2 * y1 - x1 * y2) / (x2 - x1);
                m_points.remove(newKey);
                break;
            }
        }

        setDuration(newDuration);
        emit pointsChanged();
    }
}

ProcessViewModelInterface* AutomationModel::makeViewModel(
        const id_type<ProcessViewModelInterface>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto vm = new AutomationViewModel{*this, viewModelId, parent};
    addViewModel(vm); // TODO put the addViewModel outside.
    //(should be automatically done by the interface.)
    return vm;
}

ProcessViewModelInterface* AutomationModel::cloneViewModel(
        const id_type<ProcessViewModelInterface>& newId,
        const ProcessViewModelInterface& source,
        QObject* parent)
{
    auto vm = new AutomationViewModel {
              static_cast<const AutomationViewModel&>(source), *this, newId, parent};
    addViewModel(vm);
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
    Q_ASSERT(time >= TimeValue{std::chrono::seconds(0)});
    Q_ASSERT(time <= duration());

    // Map the given time between 0 - 1
    double x = time / duration();

    // Find the two keys closest to the value.
    if(m_points.contains(x))
        return m_points[x];

    // This should be offloaded to the API.
    auto keys = m_points.keys();
    for(int i = 0; i < keys.size(); i++)
    {
        if(x > keys[i] && x < keys[i + 1])
        {
            double x1 = keys[i];
            double x2 = keys[i + 1];
            double y1 = m_points[x1];
            double y2 = m_points[x2];

            double a = (x2 - x1) / (y2 - y1);
            double b = (y1 - a * x1);

            return a * x + b;
        }
    }

    throw std::runtime_error("Should never get there");
}
