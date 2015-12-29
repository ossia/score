#pragma once
#include "src/Area/AreaFactory.hpp"

class CircleAreaFactory : public AreaFactory
{
    public:
        int type() const override;
        const AreaFactoryKey& key_impl() const override;
        QString prettyName() const override;

        QStringList generic_formula() const override;

        AreaModel* makeModel(
                const QStringList& formula,
                const Space::AreaContext& space,
                const Id<AreaModel>&,
                QObject* parent) const override;

        AreaPresenter* makePresenter(
                QGraphicsItem* view,
                const AreaModel& model,
                QObject* parent) const override;

        QGraphicsItem* makeView(QGraphicsItem* parent) const override;
};
