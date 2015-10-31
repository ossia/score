#pragma once
#include <ProcessInterface/Process.hpp>
#include <Curve/CurveModel.hpp>

class CurveProcessModel : public Process
{
        Q_OBJECT
    public:
        using Process::Process;

        CurveModel& curve() const
        { return *m_curve; }

    signals:
        void curveChanged();

    protected:
        void setCurve(CurveModel* newCurve)
        {
            delete m_curve;
            m_curve = newCurve;

            setCurve_impl();
        }

        virtual void setCurve_impl() { }

        CurveModel* m_curve{};
};
