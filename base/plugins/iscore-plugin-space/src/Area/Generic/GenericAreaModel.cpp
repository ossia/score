#include "GenericAreaModel.hpp"
#include "src/Area/AreaParser.hpp"
const AreaFactoryKey&GenericAreaModel::factoryKey() const
{
    static const AreaFactoryKey name{"Generic"};
    return name;
}

QString GenericAreaModel::prettyName() const
{
    return tr("Generic");
}

GenericAreaModel::GenericAreaModel(
        const QString& formula,
        const Space::AreaContext& space,
        const Id<AreaModel>& id,
        QObject* parent):
    AreaModel{AreaParser{formula}.result(), space, id, parent}
{
    qDebug("fuuuu");
}
