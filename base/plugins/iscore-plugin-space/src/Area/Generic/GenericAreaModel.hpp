#pragma once
#include "src/Area/AreaModel.hpp"


class GenericAreaModel : public AreaModel
{
        Q_OBJECT
    public:
        static constexpr int static_type() { return 0; }
        int type() const override { return static_type(); }

        QString factoryName() const override { return "Generic"; }
        QString prettyName() const override { return tr("Generic"); }
        static QString formula();

        GenericAreaModel(
                const QString& formula,
                const SpaceModel& space,
                const Id<AreaModel>&,
                QObject* parent);
};
