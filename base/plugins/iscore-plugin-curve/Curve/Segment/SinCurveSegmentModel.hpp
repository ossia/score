#pragma once
#include "CurveSegmentModel.hpp"

class SinCurveSegmentModel : public CurveSegmentModel
{
    public:
        using CurveSegmentModel::CurveSegmentModel;

        // TODO Factor this in a macro.
        template<typename Impl>
        SinCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveSegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveSegmentModel* clone(
                const id_type<CurveSegmentModel>& id,
                QObject* parent) const override;

        QString name() const override;
        void serialize(const VisitorVariant& vis) const override;
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) override;

        double freq = 5;
        double ampl = 0.6;
};
