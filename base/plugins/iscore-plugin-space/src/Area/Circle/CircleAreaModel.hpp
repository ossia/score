#pragma once
#include "src/Area/AreaModel.hpp"

class CircleAreaModel : public AreaModel
{
        Q_OBJECT
    public:
        static constexpr int static_type() { return 1; }
        int type() const override { return static_type(); }

        QString factoryName() const override { return "Circle"; }
        QString prettyName() const override { return tr("Circle"); }
        static QString formula();

        CircleAreaModel(
                const SpaceModel& space,
                const Id<AreaModel>&,
                QObject* parent);

        AreaPresenter *makePresenter(QGraphicsItem *, QObject *) const;
};
