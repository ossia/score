#pragma once
#include "src/Area/AreaModel.hpp"


class CircleAreaModel : public AreaModel
{
    public:
        CircleAreaModel(
                const SpaceModel& space,
                const id_type<AreaModel>&,
                QObject* parent);

        AreaPresenter *makePresenter(QGraphicsItem *, QObject *) const;
};
