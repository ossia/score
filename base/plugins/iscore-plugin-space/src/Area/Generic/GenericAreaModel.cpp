#include "GenericAreaModel.hpp"
#include "src/Area/AreaParser.hpp"


namespace Space
{
const AreaFactoryKey&GenericAreaModel::static_factoryKey()
{
    static const AreaFactoryKey name{"Generic"};
    return name;

}

const AreaFactoryKey&GenericAreaModel::factoryKey() const
{
    return static_factoryKey();
}

QString GenericAreaModel::prettyName() const
{
    return tr("Generic");
}

GenericAreaModel::GenericAreaModel(
        const QStringList& formula,
        const Space::AreaContext& space,
        const Id<AreaModel>& id,
        QObject* parent):
    AreaModel{AreaParser{formula}.result(), space, id, parent}
{
}
}
