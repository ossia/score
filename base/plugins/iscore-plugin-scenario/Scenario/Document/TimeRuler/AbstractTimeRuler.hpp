#pragma once

#include <QObject>
#include <Process/TimeValue.hpp>

class AbstractTimeRulerView;

class AbstractTimeRuler : public QObject
{
        Q_OBJECT
    public:
        explicit AbstractTimeRuler(AbstractTimeRulerView* view, QObject *parent = 0);
        virtual ~AbstractTimeRuler() = default;

        AbstractTimeRulerView* view() { return m_view; }
        int totalScroll() {return m_totalScroll;}
        TimeValue startPoint() const {return m_startPoint;}
        double pixelsPerMillis() const
        { return m_pixelPerMillis; }
        const QVector<QPair<double, TimeValue>>& graduationsSpacing() const;

    signals:

    public slots:
        virtual void setStartPoint(TimeValue dur);
        void setPixelPerMillis(double factor);

    protected:
        void computeGraduationSpacing();

        AbstractTimeRulerView* m_view;

        TimeValue m_startPoint {};

        QVector< QPair<double, TimeValue> > m_graduationsSpacing;

        double m_pixelPerMillis {0.01};
        int m_totalScroll{};
};
