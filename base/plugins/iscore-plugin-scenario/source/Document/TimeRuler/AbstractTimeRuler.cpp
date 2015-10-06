#include "AbstractTimeRuler.hpp"

#include "AbstractTimeRulerView.hpp"
#include <QGraphicsScene>
#include <QGraphicsView>
AbstractTimeRuler::AbstractTimeRuler(AbstractTimeRulerView* view, QObject* parent) :
    QObject{parent},
    m_view{view}
{
    m_graduationsSpacing.push_back( {0.2, TimeValue{std::chrono::seconds (60)} });
    m_graduationsSpacing.push_back( {0.5, TimeValue{std::chrono::seconds (30)} });

    m_graduationsSpacing.push_back( {1, TimeValue{std::chrono::seconds (10)} });
    m_graduationsSpacing.push_back( {2, TimeValue{std::chrono::seconds (5)} });
    m_graduationsSpacing.push_back( {5, TimeValue{std::chrono::seconds (2)} });

    m_graduationsSpacing.push_back( {10, TimeValue{std::chrono::seconds (1)} });
    m_graduationsSpacing.push_back( {20, TimeValue{std::chrono::milliseconds (500)} });
    m_graduationsSpacing.push_back( {40, TimeValue{std::chrono::milliseconds (250)} });
    m_graduationsSpacing.push_back( {80, TimeValue{std::chrono::milliseconds (150)} });

    m_graduationsSpacing.push_back( {100, TimeValue{std::chrono::milliseconds (100)} });
    m_graduationsSpacing.push_back( {200, TimeValue{std::chrono::milliseconds (50)} });
    m_graduationsSpacing.push_back( {500, TimeValue{std::chrono::milliseconds (20)} });
}

void AbstractTimeRuler::scroll(qreal x)
{
    /*
    QGraphicsScene * scene = m_view->scene();
    auto view = scene->views().first();

    auto viewport = view->mapToScene(view->rect()).boundingRect();
    qDebug() << x << TimeValue::fromMsecs(view->mapToScene(QPoint(0, 0)).x() / m_pixelPerMillis);
    */
    m_view->setX(-x);
//    m_totalScroll += dx;
}

void AbstractTimeRuler::setDuration(TimeValue dur)
{
    if (m_duration != dur)
    {
        m_duration = dur;
        m_view->setWidth(m_duration.msec() * m_pixelPerMillis);
    }
//    computeGraduationSpacing();
}

void AbstractTimeRuler::setStartPoint(TimeValue dur)
{
    if (m_startPoint != dur)
    {
        m_startPoint = dur;
    }
}

void AbstractTimeRuler::setPixelPerMillis(double factor)
{
    if (factor != m_pixelPerMillis)
    {
        m_view->setWidth(m_duration.msec() * factor);
//        m_view->setX(m_startPoint.msec() * factor + m_totalScroll * (factor / m_pixelPerMillis));
        m_pixelPerMillis = factor;
        computeGraduationSpacing();
    }
}

void AbstractTimeRuler::computeGraduationSpacing()
{
    double pixPerSec = 1000 * m_pixelPerMillis;
    double gradSpace = pixPerSec;

    int deltaTime = 100;
    QString format = "m''ss''''";
    int loop = 5;

    int i = 0;
    for (i = 0; i < m_graduationsSpacing.size() - 1; i++ )
    {
        if (pixPerSec > m_graduationsSpacing[i].first && pixPerSec < m_graduationsSpacing[i+1].first)
        {
            deltaTime = m_graduationsSpacing[i].second.msec();
            gradSpace = pixPerSec * m_graduationsSpacing[i].second.sec();
            break;
        }
    }
    if (i > 5)
    {
        format = "m''ss''''z";
        loop = 10;
    }

    m_view->setGraduationsStyle(gradSpace, deltaTime, format, loop );
}
QVector<QPair<double, TimeValue> > AbstractTimeRuler::graduationsSpacing() const
{
    return m_graduationsSpacing;
}
