#pragma once
#include "src/Area/AreaModel.hpp"

class CircleAreaModel : public AreaModel
{
        Q_OBJECT
    public:
        static constexpr int static_type() { return 1; }
        int type() const override { return static_type(); }

        const AreaFactoryKey& factoryKey() const override;

        QString prettyName() const override;

        static QString formula();

        CircleAreaModel(
                const Space::AreaContext& space,
                const Id<AreaModel>&,
                QObject* parent);

        AreaPresenter *makePresenter(QGraphicsItem *, QObject *) const;
};
