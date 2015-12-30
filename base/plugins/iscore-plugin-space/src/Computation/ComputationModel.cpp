#include "ComputationModel.hpp"

ComputationModel::ComputationModel(
        const QString& name,
        const Computation& comp,
        const SpaceModel& space,
        const Id<ComputationModel>& id,
        QObject* parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_name{name},
    m_fun{comp},
    m_space{space}
{

}
