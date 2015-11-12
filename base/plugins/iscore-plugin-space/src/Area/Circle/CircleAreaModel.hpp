#pragma once
#include "src/Area/AreaModel.hpp"

class CircleAreaModel : public AreaModel
{
        Q_OBJECT
    public:
        static constexpr int static_type() { return 1; }
        int type() const override { return static_type(); }

        const std::string& factoryName() const override {
            static const std::string name{"Circle"};
            return name;
        }

        QString prettyName() const override { return tr("Circle"); }
        static QString formula();

        CircleAreaModel(
                const SpaceModel& space,
                const Id<AreaModel>&,
                QObject* parent);

        AreaPresenter *makePresenter(QGraphicsItem *, QObject *) const;
};
