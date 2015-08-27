#include "ComputationModel.hpp"

ComputationModel::ComputationModel(
        std::unique_ptr<spacelib::computation>&& computation,
        const SpaceModel& space,
        const Id<ComputationModel>& id,
        QObject* parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_space{space},
    m_computation{std::move(computation)}
{

}
