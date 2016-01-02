#pragma once
#include <boost/container/flat_map.hpp>
#include <QPoint>
#include <QVariant>
#include <QVector>
#include <memory>
#include <utility>
#include <vector>

#include <Curve/Segment/CurveSegmentFactoryKey.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
class LinearSegment;
struct SegmentData;
struct PointArraySegmentData
{
    double min_x, max_x;
    double min_y, max_y;
    QVector<QPointF> m_points;
};
class ISCORE_PLUGIN_CURVE_EXPORT PointArraySegment final : public SegmentModel
{
        Q_OBJECT
    public:
        using data_type = PointArraySegmentData;
        using SegmentModel::SegmentModel;
        PointArraySegment(
                const SegmentData& dat,
                QObject* parent);

        template<typename Impl>
        PointArraySegment(Deserializer<Impl>& vis, QObject* parent) :
            SegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        SegmentModel* clone(
                const Id<SegmentModel>& id,
                QObject* parent) const override;

        const SegmentFactoryKey& key() const override;
        void serialize(const VisitorVariant& vis) const override;
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        void addPoint(double, double);
        void simplify();
        [[ deprecated ]] std::vector<std::unique_ptr<LinearSegment>> piecewise() const;
        std::vector<SegmentData> toLinearSegments() const;

        double min() { return min_y; }
        double max() { return max_y; }

        QVariant toSegmentSpecificData() const override
        {
            PointArraySegmentData dat{
                           min_x, max_x,
                           min_y, max_y, {}};

            dat.m_points.reserve(m_points.size());
            for(const auto& pt : m_points)
                dat.m_points.push_back({pt.first, pt.second});

            return QVariant::fromValue(std::move(dat));
        }

    signals:
        void minChanged(double);
        void maxChanged(double);

    private:
        // Coordinates in {x, y}.
        double min_x{}, max_x{};
        double min_y{}, max_y{};

        boost::container::flat_map<double, double> m_points;
};
}


Q_DECLARE_METATYPE(Curve::PointArraySegmentData)

