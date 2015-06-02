#pragma once
#include "CurveSegmentModel.hpp"
// ALl between zero and one.
class LinearCurveSegmentModel : public CurveSegmentModel
{
    public:
        using CurveSegmentModel::CurveSegmentModel;

        // TODO Factor this in a macro.
        template<typename Impl>
        LinearCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveSegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveSegmentModel* clone(
                const id_type<CurveSegmentModel>& id,
                QObject* parent) const override;

        QString name() const override;
        void serialize(const VisitorVariant& vis) const;
        void on_startChanged() override;
        void on_endChanged() override;

        virtual QVector<QPointF> data(int numInterp) const override;
};


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
        void serialize(const VisitorVariant& vis) const;
        void on_startChanged() override;
        void on_endChanged() override;

        virtual QVector<QPointF> data(int numInterp) const override;

    private:
        double gamma = 0.5;
};

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
        void serialize(const VisitorVariant& vis) const;
        void on_startChanged() override;
        void on_endChanged() override;

        virtual QVector<QPointF> data(int numInterp) const override;

    private:
        double freq = 5;
        double ampl = 0.6;
};
