#pragma once
#include <Curve/CurveModel.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Process/Process.hpp>
#include <qstring.h>

#include "Curve/Segment/CurveSegmentModel.hpp"
#include "iscore/selection/Selection.hpp"

class CurveProcessModel : public Process
{
        Q_OBJECT
    public:
        using Process::Process;

        CurveModel& curve() const
        { return *m_curve; }

        virtual ~CurveProcessModel();


        void startExecution() final override
        {
            emit execution(true);
        }

        void stopExecution() final override
        {
            emit execution(false);
        }

        void reset() final override
        {

        }


        Selection selectableChildren() const final override
        {
            Selection s;
            for(auto& segment : m_curve->segments())
                s.append(&segment);
            for(auto& point : m_curve->points())
                s.append(point);
            return s;
        }

        Selection selectedChildren() const final override
        {
            return m_curve->selectedChildren();
        }

        void setSelection(const Selection & s) const final override
        {
            m_curve->setSelection(s);
        }


    signals:
        void curveChanged();

    protected:
        void setCurve(CurveModel* newCurve)
        {
            delete m_curve;
            m_curve = newCurve;

            setCurve_impl();

            emit m_curve->changed();
        }

        virtual void setCurve_impl() { }

        CurveModel* m_curve{};
};
