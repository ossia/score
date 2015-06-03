#pragma once
#include "CurveSegmentFactory.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

// TODO candidate to template
class LinearCurveSegmentFactory : public CurveSegmentFactory
{
        // CurveSegmentFactory interface
    public:
        QString name() const;

        CurveSegmentModel *make(
                const id_type<CurveSegmentModel>& id,
                QObject* parent) override;

        CurveSegmentModel *load(
                const VisitorVariant& vis,
                QObject* parent) override;
};

class GammaCurveSegmentFactory : public CurveSegmentFactory
{
        // CurveSegmentFactory interface
    public:
        QString name() const;

        CurveSegmentModel *make(
                const id_type<CurveSegmentModel>& id,
                QObject* parent) override;

        CurveSegmentModel *load(
                const VisitorVariant& vis,
                QObject* parent) override;
};

class SinCurveSegmentFactory : public CurveSegmentFactory
{
        // CurveSegmentFactory interface
    public:
        QString name() const;

        CurveSegmentModel *make(
                const id_type<CurveSegmentModel>& id,
                QObject* parent) override;

        CurveSegmentModel *load(
                const VisitorVariant& vis,
                QObject* parent) override;
};
