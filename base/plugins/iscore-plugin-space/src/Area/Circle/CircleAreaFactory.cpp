#include "CircleAreaFactory.hpp"
#include "CircleAreaModel.hpp"
#include "CircleAreaPresenter.hpp"
#include "CircleAreaView.hpp"

int CircleAreaFactory::type() const
{
    return CircleAreaModel::static_type();
}

const std::string& CircleAreaFactory::key_impl() const
{
    static const std::string name{"Circle"};
    return name;
}

QString CircleAreaFactory::prettyName() const
{
    return QObject::tr("Circle");
}

AreaModel*CircleAreaFactory::makeModel(
        const QString& formula,
        const SpaceModel& space,
        const Id<AreaModel>& id,
        QObject* parent) const
{
    return new CircleAreaModel{space, id, parent};
}

QString CircleAreaFactory::generic_formula() const
{
    return {CircleAreaModel::formula()};
}


AreaPresenter*CircleAreaFactory::makePresenter(
        QGraphicsItem* view,
        const AreaModel& model,
        QObject* parent) const
{
    return new CircleAreaPresenter{
        static_cast<CircleAreaPresenter::view_type*>(view),
                static_cast<const CircleAreaPresenter::model_type&>(model), parent};
}

QGraphicsItem* CircleAreaFactory::makeView(QGraphicsItem* parent) const
{
    return new CircleAreaView{parent};
}
