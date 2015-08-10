#pragma once
#include "src/Area/AreaModel.hpp"


class CircleAreaModel : public AreaModel
{
    public:
        static constexpr int static_type() { return 1; }
        static QString pretty_name() { return tr("Circle"); }
        static QString formula();
        virtual int type() const override { return 1; }
        CircleAreaModel(
                const SpaceModel& space,
                const id_type<AreaModel>&,
                QObject* parent);

        AreaPresenter *makePresenter(QGraphicsItem *, QObject *) const;
};
