#include "ComputationModel.hpp"

ComputationModel::ComputationModel(
        std::unique_ptr<spacelib::computation>&& computation,
        const SpaceModel& space,
        const id_type<ComputationModel>& id,
        QObject* parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_space{space},
    m_computation{std::move(computation)}
{

}
