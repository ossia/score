#pragma once
#include "src/Area/AreaModel.hpp"

namespace Space
{
class PointerAreaModel : public AreaModel
{
        Q_OBJECT
    public:
        static constexpr int static_type() { return 2; }
        int type() const override { return static_type(); }

        static const AreaFactoryKey& static_factoryKey();
        const AreaFactoryKey& factoryKey() const override;

        QString prettyName() const override;

        static QStringList formula();

        struct values {
                double x;
                double y;
        };

        static auto mapToData(
                const ValMap& map)
        {
            return values{map.at("x0"), map.at("y0")};
        }

        PointerAreaModel(
                const Space::AreaContext& space,
                const Id<AreaModel>&,
                QObject* parent);

        AreaPresenter *makePresenter(QGraphicsItem *, QObject *) const;
};
}
