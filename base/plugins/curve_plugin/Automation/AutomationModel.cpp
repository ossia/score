#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"

AutomationModel::AutomationModel(id_type<ProcessSharedModelInterface> id,
                                 QObject* parent) :
    ProcessSharedModelInterface {id, "Automation", parent}
{

    // Demo
    addPoint(0, 0);
    addPoint(1, 1);
}

ProcessSharedModelInterface* AutomationModel::clone(id_type<ProcessSharedModelInterface> newId,
        QObject* newParent)
{
    auto autom = new AutomationModel {newId, newParent};
    autom->setAddress(address());
    autom->setPoints(QMap<double, double> {points() });

    return autom;
}

QString AutomationModel::processName() const
{
    return "Automation";
}

void AutomationModel::setDurationWithScale(TimeValue newDuration)
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

void AutomationModel::setDurationWithoutScale(TimeValue newDuration)
{
    // The duration can only grow from the outside (constraint scaling), not shrink
    // TODO, shrink (cut) the process.

    if(newDuration > duration())
    {
        double scale = duration().msec() / double(newDuration.msec());
        auto points = m_points;
        auto keys = m_points.keys();
        m_points.clear();

        // 1. Scale the x axis by multiplying each x value.
        for(int i = 0; i < keys.size(); ++i)
        {
            m_points[keys[i] * scale] = points[keys[i]];
        }

        // 2. Create a new point at the end (x=1) with the same value than the last point.
        auto last = m_points.last();
        m_points[1] = last;


        setDuration(newDuration);
        emit pointsChanged();
    }
}

ProcessViewModelInterface* AutomationModel::makeViewModel(id_type<ProcessViewModelInterface> viewModelId,
        QObject* parent)
{
    return new AutomationViewModel {this, viewModelId, parent};
}

ProcessViewModelInterface* AutomationModel::makeViewModel(id_type<ProcessViewModelInterface> newId,
        const ProcessViewModelInterface* source,
        QObject* parent)
{
    return new AutomationViewModel {static_cast<const AutomationViewModel*>(source), this, newId, parent};
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
