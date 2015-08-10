#include "GenericAreaFactory.hpp"
#include "GenericAreaModel.hpp"
#include "GenericAreaPresenter.hpp"
#include "GenericAreaView.hpp"

int GenericAreaFactory::type() const
{
    return GenericAreaModel::static_type();
}

QString GenericAreaFactory::name() const
{
    return "Generic";
}

QString GenericAreaFactory::prettyName() const
{
    return QObject::tr("Generic");
}

AreaModel*GenericAreaFactory::makeModel(
        const QString& formula,
        const SpaceModel& space,
        const id_type<AreaModel>& id,
        QObject* parent) const
{
    return new GenericAreaModel{formula, space, id, parent};
}

QString GenericAreaFactory::generic_formula() const
{
    return {};
}


AreaPresenter*GenericAreaFactory::makePresenter(
        QGraphicsItem* view,
        const AreaModel& model,
        QObject* parent) const
{
    return new GenericAreaPresenter{
        static_cast<GenericAreaPresenter::view_type*>(view),
                static_cast<const GenericAreaPresenter::model_type&>(model), parent};
}

QGraphicsItem* GenericAreaFactory::makeView(QGraphicsItem* parent) const
{
    return new GenericAreaView{parent};
}
