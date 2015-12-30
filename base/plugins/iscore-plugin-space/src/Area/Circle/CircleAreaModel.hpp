#pragma once
#include "src/Area/AreaModel.hpp"

class CircleAreaModel : public AreaModel
{
        Q_OBJECT
    public:
        static constexpr int static_type() { return 1; }
        int type() const override { return static_type(); }

        static const AreaFactoryKey& static_factoryKey();
        const AreaFactoryKey& factoryKey() const override;

        QString prettyName() const override;

        static QStringList formula();

        struct values {
                double x;
                double y;
                double r;
        };

        static auto mapToData(
                GiNaC::exmap map,
                const ParameterMap& pm)
        {
            return values{GiNaC::ex_to<GiNaC::numeric>(map.at(pm["x0"].first)).to_double(),
                        GiNaC::ex_to<GiNaC::numeric>(map.at(pm["y0"].first)).to_double(),
                        GiNaC::ex_to<GiNaC::numeric>(map.at(pm["r"].first)).to_double()};
        }

        CircleAreaModel(
                const Space::AreaContext& space,
                const Id<AreaModel>&,
                QObject* parent);

        AreaPresenter *makePresenter(QGraphicsItem *, QObject *) const;
};
