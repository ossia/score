#pragma once
#include "src/Area/AreaModel.hpp"


class GenericAreaModel : public AreaModel
{
        Q_OBJECT
    public:
        static constexpr int static_type() { return 0; }
        int type() const override { return static_type(); }

        const std::string& factoryName() const override {
            static const std::string name{"Generic"};
            return name;
        }

        QString prettyName() const override { return tr("Generic"); }
        static QString formula();

        GenericAreaModel(
                const QString& formula,
                const SpaceModel& space,
                const Id<AreaModel>&,
                QObject* parent);
};
