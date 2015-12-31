#pragma once
#include <QVariant>

#include <Curve/Segment/CurveSegmentFactoryKey.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
struct ISCORE_PLUGIN_CURVE_EXPORT LinearSegmentData
{
        static const SegmentFactoryKey& key();
};


class ISCORE_PLUGIN_CURVE_EXPORT LinearSegment final : public SegmentModel
{
    public:
        using data_type = LinearSegmentData;
        using SegmentModel::SegmentModel;

        template<typename Impl>
        LinearSegment(Deserializer<Impl>& vis, QObject* parent) :
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

        QVariant toSegmentSpecificData() const override;
};
}

Q_DECLARE_METATYPE(Curve::LinearSegmentData)
