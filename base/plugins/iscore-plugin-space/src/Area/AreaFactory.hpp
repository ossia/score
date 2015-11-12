#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QObject>
class QGraphicsItem;
class SpaceModel;
class AreaModel;
class AreaPresenter;

class AreaTag{};
using AreaFactoryKey = StringFactoryKey<AreaTag>;
Q_DECLARE_METATYPE(AreaFactoryKey)

class AreaFactory : public iscore::GenericFactoryInterface<AreaFactoryKey>
{
        ISCORE_FACTORY_DECL("Area")
    public:
        virtual ~AreaFactory();

        // Pretty name, id
        virtual int type() const = 0;

        virtual QString prettyName() const = 0;

        // Model
        virtual AreaModel* makeModel(
                const QString& generic_formula,
                const SpaceModel& space,
                const Id<AreaModel>&,
                QObject* parent) const = 0;

        // Presenter
        virtual AreaPresenter* makePresenter(
                QGraphicsItem* view,
                const AreaModel& model,
                QObject* parent) const = 0;

        // View
        virtual QGraphicsItem* makeView(QGraphicsItem* parent) const = 0;

        // Formula
        virtual QString generic_formula() const = 0;

        // Widget ?
};
