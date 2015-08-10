#pragma once
#include "src/Area/AreaFactory.hpp"

class CircleAreaFactory : public AreaFactory
{
    public:
        int type() const override;
        QString name() const override;
        QString prettyName() const override;

        QString generic_formula() const override;

        AreaModel* makeModel(
                const QString& formula,
                const SpaceModel& space,
                const id_type<AreaModel>&,
                QObject* parent) const override;

        AreaPresenter* makePresenter(
                QGraphicsItem* view,
                const AreaModel& model,
                QObject* parent) const override;

        QGraphicsItem* makeView(QGraphicsItem* parent) const override;
};
