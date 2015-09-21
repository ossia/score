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
        void scroll(qreal x);
        int totalScroll() {return m_totalScroll;}
        TimeValue startPoint() const {return m_startPoint;}

        QVector<QPair<double, TimeValue> > graduationsSpacing() const;

signals:

public slots:
        void setDuration(TimeValue dur);
        virtual void setStartPoint(TimeValue dur);
        void setPixelPerMillis(double factor);

    protected:
        void computeGraduationSpacing();

        AbstractTimeRulerView* m_view;

        TimeValue m_startPoint {};
        TimeValue m_duration {};

        QVector< QPair<double, TimeValue> > m_graduationsSpacing;

        double m_pixelPerMillis {0.01};
        int m_totalScroll{};
};
