#include "ComputationModel.hpp"
namespace Space
{
ComputationModel::ComputationModel(
        const Computation& comp,
        const SpaceModel& space,
        const Id<ComputationModel>& id,
        QObject* parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_fun{comp},
    m_space{space}
{
    metadata.setName(QString("Computation.%1").arg(*this->id().val()));

}
}
