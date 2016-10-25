#pragma once
#include <boost/container/flat_map.hpp>
#include <QPoint>
#include <QVariant>
#include <QVector>
#include <memory>
#include <utility>
#include <vector>


#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
class PointArraySegment;
}

CURVE_SEGMENT_METADATA(
        ISCORE_PLUGIN_CURVE_EXPORT,
        Curve::PointArraySegment,
        "c598b840-db67-4c8f-937a-46cfac87cb59",
        "PointArray",
        "PointArray")

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
class ISCORE_PLUGIN_CURVE_EXPORT PointArraySegment final :
        public SegmentModel
{
        Q_OBJECT
        MODEL_METADATA_IMPL(PointArraySegment)
    public:
        using data_type = PointArraySegmentData;
        PointArraySegment(
            const Id<SegmentModel>& id,
            QObject* parent): SegmentModel{id, parent} { }
        PointArraySegment(
                const SegmentData& dat,
                QObject* parent);

        PointArraySegment(
                    const PointArraySegment& other,
                    const id_type& id,
                    QObject* parent);

        template<typename Impl>
        PointArraySegment(Deserializer<Impl>& vis, QObject* parent) :
            SegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        void addPoint(double, double);
        void simplify(double ratio); // 10 is a good ratio
        std::vector<SegmentData> toLinearSegments() const;
        std::vector<SegmentData> toPowerSegments() const;

        double min() { return min_y; }
        double max() { return max_y; }


        const boost::container::flat_map<double, double>& points() const
        { return m_points; }

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

        // This will throw if execution is attempted.
        std::function<float(double, float, float)> makeFloatFunction() const override
        { return {}; }
        std::function<int(double, int, int)> makeIntFunction() const override
        { return {}; }
        std::function<bool(double, bool, bool)> makeBoolFunction() const override
        { return {}; }

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

