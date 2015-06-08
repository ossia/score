#pragma once
#include "CurveSegmentModel.hpp"

class GammaCurveSegmentModel : public CurveSegmentModel
{
    public:
        using CurveSegmentModel::CurveSegmentModel;

        // TODO Factor this in a macro.
        template<typename Impl>
        GammaCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
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

        boost::optional<double> verticalParameter();
        void setVerticalParameter(double p);

        double gamma = 0.5;

};
