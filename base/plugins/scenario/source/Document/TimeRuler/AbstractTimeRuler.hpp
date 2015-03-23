#pragma once

#include <QObject>
#include "ProcessInterface/TimeValue.hpp"

class AbstractTimeRulerView;

class AbstractTimeRuler : public QObject
{
        Q_OBJECT
    public:
        explicit AbstractTimeRuler(AbstractTimeRulerView* view, QObject *parent = 0);
        virtual ~AbstractTimeRuler() = default;

        AbstractTimeRulerView* view() { return m_view; }
        void setOffset(TimeValue t) {m_offset = t;}
        TimeValue offset() {return m_offset;}
        void setRelativeOffset(int ms) {m_offset.addMSecs(ms);}
        void scroll(int dx);

    signals:

    public slots:
        void setDuration(TimeValue dur);
        void setStartPoint(TimeValue dur);
        void setPixelPerMillis(double factor);

    protected:
        void computeGraduationSpacing();

        AbstractTimeRulerView* m_view;

        TimeValue m_startPoint {};
        TimeValue m_duration {};

        QVector< QPair<double, TimeValue> > m_graduationsSpacing;

        double m_pixelPerMillis {0.01};
        TimeValue m_offset{std::chrono::milliseconds(0)};
};
