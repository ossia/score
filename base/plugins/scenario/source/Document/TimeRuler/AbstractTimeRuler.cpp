#include "AbstractTimeRuler.hpp"

#include "AbstractTimeRulerView.hpp"

AbstractTimeRuler::AbstractTimeRuler(AbstractTimeRulerView* view, QObject *parent) :
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

void AbstractTimeRuler::scroll(int dx)
{
    view()->setX(view()->x() + dx);
}

void AbstractTimeRuler::setDuration(TimeValue dur)
{
    if (m_duration != dur)
    {
        m_duration = dur;
        m_view->setWidth(m_duration.msec() * m_pixelPerMillis);
    }
}

void AbstractTimeRuler::setStartPoint(TimeValue dur)
{
    if (m_startPoint != dur)
    {
        m_startPoint = dur;
        m_view->setX((m_startPoint + m_offset).msec() * m_pixelPerMillis);
    }
}

void AbstractTimeRuler::setPixelPerMillis(double factor)
{
    if (factor != m_pixelPerMillis)
    {
        m_pixelPerMillis = factor;
        computeGraduationSpacing();
        m_view->setWidth(m_duration.msec() * m_pixelPerMillis);
        m_view->setX(m_startPoint.msec() * m_pixelPerMillis);
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

