#pragma once
#include "Curve/Segment/CurveSegmentModel.hpp"
class LinearCurveSegmentData
{

};

Q_DECLARE_METATYPE(LinearCurveSegmentData)

class LinearCurveSegmentModel final : public CurveSegmentModel
{
    public:
        using data_type = LinearCurveSegmentData;
        using CurveSegmentModel::CurveSegmentModel;

        template<typename Impl>
        LinearCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
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

        QVariant toSegmentSpecificData() const override;
};
