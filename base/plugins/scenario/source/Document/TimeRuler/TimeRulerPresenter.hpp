#pragma once

#include <QObject>
#include "ProcessInterface/TimeValue.hpp"

class TimeRulerView;

class TimeRulerPresenter : public QObject
{
        Q_OBJECT
    public:
        explicit TimeRulerPresenter(TimeRulerView* view, QObject *parent = 0);
        ~TimeRulerPresenter();

        TimeRulerView* view() { return m_view; }

    signals:

    public slots:
        void setDuration(TimeValue dur);
        void setStartPoint(TimeValue dur);
        void setPixelPerMillis(double factor);

    private:
        void computeGraduationSpacing();

        TimeRulerView* m_view;

        TimeValue m_startPoint {};
        TimeValue m_duration {};

        QVector< QPair<double, TimeValue> > m_graduationsSpacing;

        double m_pixelPerMillis {0.01};
};
