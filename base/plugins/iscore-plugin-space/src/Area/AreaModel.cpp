#include "AreaModel.hpp"
AreaModel::AreaModel(
        std::unique_ptr<spacelib::area>&& area,
        const SpaceModel& space,
        const id_type<AreaModel> & id,
        QObject *parent):
    IdentifiedObject{id, "AreaModel", parent},
    m_space{space},
    m_area{std::move(area)}
{

}

void AreaModel::setArea(std::unique_ptr<spacelib::area> &&ar)
{
    m_area = std::move(ar);
}
