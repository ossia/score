#pragma once
#include "Curve/Segment/CurveSegmentModel.hpp"

class PointArrayCurveSegmentModel : public CurveSegmentModel
{
        Q_OBJECT
    public:
        using CurveSegmentModel::CurveSegmentModel;

        template<typename Impl>
        PointArrayCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveSegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveSegmentModel* clone(
                const Id<CurveSegmentModel>& id,
                QObject* parent) const override;

        QString name() const override;
        void serialize(const VisitorVariant& vis) const override;
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        void addPoint(double, double);

        double min_x{}, max_x{};
        double min_y{}, max_y{};
        // Coordinates in {x, y}.

    signals:
        void minChanged(double);
        void maxChanged(double);

    private:
        std::map<double, double> m_points;
};
