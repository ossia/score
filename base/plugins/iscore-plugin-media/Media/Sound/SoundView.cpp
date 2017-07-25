#include "SoundView.hpp"
#include <QPainter>
#include <QGraphicsView>
#include <QScrollBar>
#include <cmath>
#include <QGraphicsSceneContextMenuEvent>
#if defined(__SSE2__)
#include <xmmintrin.h>
#endif
namespace Media
{
namespace Sound
{

LayerView::LayerView(QGraphicsItem* parent):
    Process::LayerView{parent}
{
    setFlag(ItemClipsToShape, true);
    auto view = scene()->views().first();
    connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, &Media::Sound::LayerView::scrollValueChanged);
}

long LayerView::compareDensity(const double density) {

    if (m_density == -1 || m_density >= 4 * density || 4 * m_density <= density || (int)width() == 0 || m_curdata.empty()) {
        return RECOMPUTE_ALL;
    }
    if (m_density >= 2 * density) {
        return USE_NEXT;
    }
    if (2 * m_density <= density) {
        return USE_PREV;
    }
    return KEEP_CUR;
}

void LayerView::printAction(long action) {
    switch (action) {
    case KEEP_CUR:
        qDebug() << "keep current";
        break;
    case USE_NEXT :
        qDebug() << "use next";
        break;
    case USE_PREV :
        qDebug() << "use prev";
        break;
    case RECOMPUTE_ALL :
        qDebug() << "recompute all";
        break;
    default :
        qDebug() << "unknown action";
    }
}

void LayerView::computeDataSet(ZoomRatio ratio, double* densityptr, std::vector<std::vector<float>>& dataset)
{
    const int nchannels = m_data.size();

    const int density = std::max((m_sampleRate * ratio) / 1000., 1.);

    if (densityptr != nullptr)
        *densityptr = density;

    dataset.resize(nchannels);
    for (int c = 0; c < nchannels; ++c) {

        const auto& chan = m_data[c];
        const int chan_n = chan.size();

        const double length = double(1000ll * chan_n) / m_sampleRate; // duration of the track
        const double size = ratio > 0 ? length / ratio : 0; // number of pixels the track will occupy in its entirety

        const int npoints = size;
        std::vector<float>& rmsv = dataset[c];
        rmsv.resize(npoints);

        const float one_over_dens = 1. / density;
        for (int i = 0; i < npoints; ++i)
        {
            rmsv[i] = 0;
            for (int j = 0;
                 (j < density) && ((i * density + j) < chan_n);
                 ++j)
            {
                auto s = chan[i * density + j];
                rmsv[i] += s * s;
            }
        }

        for(int i = 0; i < npoints; i++)
        {
            rmsv[i] = std::sqrt(rmsv[i] * one_over_dens);
        }
    }
}

void LayerView::setData(const MediaFileHandle& data)
{
    m_data = data.data();
    m_sampleRate = data.sampleRate();
    m_density = -1;
}

void LayerView::drawWaveForms(ZoomRatio ratio) {

    const double density = std::max((ratio * m_sampleRate) / 1000., 1.);
    const double densityratio = (m_density > 0 && density > 0) ? m_density / density : 1.;

    int nchannels = m_data.size();
    if (nchannels == 0)
        return;

    // Height of each channel
    const auto h = height() / (double)nchannels;

    const int64_t w = width();

    // Trace lines between channels

    for (int c = 1 ; c < nchannels; ++c) {
        m_channels.moveTo(0, c * h);
        m_channels.lineTo(w, c * h);
    }

    // Get horizontal offset

    auto view = scene()->views().first();
    auto x0 = std::max(mapFromScene(view->mapToScene(0, 0)).x(), qreal(0));

    int64_t i0 = x0 / densityratio;
    const int64_t n = m_curdata[0].size();
    if(n == 0)
        return;

    auto xf = mapFromScene(view->mapToScene(view->width(), 0)).x();

   for (int64_t c = 0; c < nchannels ; ++c) {
        const int64_t current_height = c * h;
        const std::vector<float>& dataset = m_curdata[c];

        QPainterPath path{};
        path.setFillRule(Qt::WindingFill);

        // Draw path for current channel

        auto height_adjustemnt = current_height + h / 2.;
        if (n > i0) {
            path.moveTo(x0, dataset[i0] + height_adjustemnt);
            double x = x0;
            for (int64_t i = i0; (i < n) && (x <= xf); ++i) {
                x = i * densityratio;
                path.lineTo(x, dataset[i] * h / 2. + height_adjustemnt);
            }
            path.lineTo(x, height_adjustemnt);
        }
        m_paths.push_back(path);
   }
}

void LayerView::recompute(const TimeVal& dur, ZoomRatio ratio)
{
    m_zoom = ratio;
    m_paths = QList<QPainterPath> ();
    m_channels = QPainterPath{};

    if(m_data.size() == 0)
        return;

    const int64_t density = std::max((int)(ratio * m_sampleRate / 1000), 1);
    long action = compareDensity(density);

    switch (action) {
    case KEEP_CUR:
        break;
    case USE_NEXT:
        std::swap(m_prevdata, m_curdata);
        std::swap(m_curdata, m_nextdata);
        m_prevdensity = m_density;
        m_density = m_nextdensity;
        if (density > 1)
            computeDataSet(ratio / 2., &m_nextdensity, m_nextdata);
        else
          m_nextdata = m_curdata;
        break;
    case USE_PREV:
        std::swap(m_nextdata, m_curdata);
        std::swap(m_curdata, m_prevdata);
        m_nextdensity = m_density;
        m_density = m_prevdensity;
        computeDataSet(2. * ratio, &m_prevdensity, m_prevdata);
        break;
    case RECOMPUTE_ALL:
        computeDataSet(ratio, &m_density, m_curdata);
        computeDataSet(2. * ratio, &m_prevdensity, m_prevdata);
        computeDataSet(ratio / 2., &m_nextdensity, m_nextdata);
        break;
    default:
        break;
    }

    drawWaveForms(ratio);

    update();
}

void LayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    emit askContextMenu(event->screenPos(), event->scenePos());

    event->accept();
}

void LayerView::paint_impl(QPainter* painter) const
{
    const int nchannels = m_data.size();
    if (nchannels == 0)
        return;

    painter->setBrush(Qt::darkCyan);
    painter->setPen(Qt::darkBlue);

    for (const auto& path : m_paths)
        painter->drawPath(path);

    painter->save();

    const auto h = -height() / nchannels;
    const auto dblh = 2. * h;

    painter->scale(1, -1);
    painter->translate(0, h + 1);

    for (const auto& path : m_paths) {
        painter->drawPath(path);
        painter->translate(0., dblh + 1);
    }

    painter->restore();

    painter->setPen(Qt::lightGray);
    painter->drawPath(m_channels);
}

void LayerView::scrollValueChanged(int sbvalue) {
    recompute(TimeVal(), m_zoom);
}

void LayerView::mousePressEvent(QGraphicsSceneMouseEvent*)
{
    emit pressed();
}
}
}
