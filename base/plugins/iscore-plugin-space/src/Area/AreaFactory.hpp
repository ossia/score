#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QObject>
class QGraphicsItem;
class SpaceModel;
class AreaModel;
class AreaPresenter;

class AreaFactory : public iscore::FactoryInterface
{
    public:
        // Pretty name, id
        virtual int type() const = 0;
        virtual QString name() const = 0;
        virtual QString prettyName() const = 0;

        // Model
        virtual AreaModel* makeModel(
                const QString& generic_formula,
                const SpaceModel& space,
                const id_type<AreaModel>&,
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
