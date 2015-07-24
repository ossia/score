#include "AreaModel.hpp"
AreaModel::AreaModel(
        const SpaceModel& space,
        const id_type<AreaModel> & id,
        QObject *parent):
    IdentifiedObject{id, "AreaModel", parent},
    m_space{space}
{

}

void AreaModel::setArea(std::unique_ptr<spacelib::area> &&ar)
{
    m_area = std::move(ar);
}
