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

class LinearCurveSegmentModel;
class QObject;
struct CurveSegmentData;
#include <iscore/tools/SettableIdentifier.hpp>

struct PointArrayCurveSegmentData
{
    double min_x, max_x;
    double min_y, max_y;
    QVector<QPointF> m_points;
};

Q_DECLARE_METATYPE(PointArrayCurveSegmentData)

class ISCORE_PLUGIN_CURVE_EXPORT PointArrayCurveSegmentModel final : public CurveSegmentModel
{
        Q_OBJECT
    public:
        using data_type = PointArrayCurveSegmentData;
        using CurveSegmentModel::CurveSegmentModel;
        PointArrayCurveSegmentModel(
                const CurveSegmentData& dat,
                QObject* parent);

        template<typename Impl>
        PointArrayCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveSegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveSegmentModel* clone(
                const Id<CurveSegmentModel>& id,
                QObject* parent) const override;

        const CurveSegmentFactoryKey& key() const override;
        void serialize(const VisitorVariant& vis) const override;
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        void addPoint(double, double);
        void simplify();
        [[ deprecated ]] std::vector<std::unique_ptr<LinearCurveSegmentModel>> piecewise() const;
        std::vector<CurveSegmentData> toLinearSegments() const;

        double min() { return min_y; }
        double max() { return max_y; }

        QVariant toSegmentSpecificData() const override
        {
            PointArrayCurveSegmentData dat{
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
