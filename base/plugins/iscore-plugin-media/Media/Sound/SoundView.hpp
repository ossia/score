#pragma once
#include <Process/LayerView.hpp>
#include <Media/AudioArray.hpp>
#include <Process/TimeValue.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Process/ZoomHelper.hpp>

namespace Media
{
namespace Sound
{
class LayerView final : public Process::LayerView
{
        Q_OBJECT
    public:
        explicit LayerView(QGraphicsItem* parent);

        void setData(const MediaFileHandle& data);
        void recompute(const TimeVal& dur, ZoomRatio ratio);
    signals:
        void pressed();
        void askContextMenu(QPoint, QPointF);
    private:

        void contextMenuEvent(
            QGraphicsSceneContextMenuEvent* event) override;
        void paint_impl(QPainter*) const override;
        void mousePressEvent(QGraphicsSceneMouseEvent*) override;

        // Returns what to do depending on current density and stored density
        long compareDensity(const double);

        // Computes a data set for the given ZoomRatio
        std::vector<std::vector<double> > computeDataSet(ZoomRatio, double* ptr = nullptr);

        void drawWaveForms(ZoomRatio);
        void scrollValueChanged(int);

        AudioArray m_data{};
        QList<QPainterPath> m_paths;
        QPainterPath m_channels{};
        int m_sampleRate;

        double m_prevdensity = -1;
        double m_density = -1;
        double m_nextdensity = -1;

        ZoomRatio m_zoom;

        std::vector<std::vector<double> > m_prevdata;
        std::vector<std::vector<double> > m_curdata;
        std::vector<std::vector<double> > m_nextdata;

        enum {KEEP_CUR = 0, USE_PREV, USE_NEXT, RECOMPUTE_ALL};
        void printAction(long);
};
}
}
