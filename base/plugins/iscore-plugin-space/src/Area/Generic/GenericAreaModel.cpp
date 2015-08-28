#include "GenericAreaModel.hpp"
#include "src/Area/AreaParser.hpp"
GenericAreaModel::GenericAreaModel(
        const QString& formula,
        const SpaceModel& space,
        const Id<AreaModel>& id,
        QObject* parent):
    AreaModel{AreaParser{formula}.result(), space, id, parent}
{

}
