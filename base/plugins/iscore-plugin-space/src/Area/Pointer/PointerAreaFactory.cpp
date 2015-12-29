#include "PointerAreaFactory.hpp"
#include "PointerAreaModel.hpp"
#include "PointerAreaPresenter.hpp"
#include "PointerAreaView.hpp"

int PointerAreaFactory::type() const
{
    return PointerAreaModel::static_type();
}

const AreaFactoryKey& PointerAreaFactory::key_impl() const
{
    static const AreaFactoryKey name{"Pointer"};
    return name;
}

QString PointerAreaFactory::prettyName() const
{
    return QObject::tr("Pointer");
}

AreaModel*PointerAreaFactory::makeModel(
        const QStringList& formula,
        const Space::AreaContext& space,
        const Id<AreaModel>& id,
        QObject* parent) const
{
    return new PointerAreaModel{space, id, parent};
}

QStringList PointerAreaFactory::generic_formula() const
{
    return PointerAreaModel::formula();
}


AreaPresenter*PointerAreaFactory::makePresenter(
        QGraphicsItem* view,
        const AreaModel& model,
        QObject* parent) const
{
    return new PointerAreaPresenter{
        static_cast<PointerAreaPresenter::view_type*>(view),
                static_cast<const PointerAreaPresenter::model_type&>(model), parent};
}

QGraphicsItem* PointerAreaFactory::makeView(QGraphicsItem* parent) const
{
    return new PointerAreaView{parent};
}
