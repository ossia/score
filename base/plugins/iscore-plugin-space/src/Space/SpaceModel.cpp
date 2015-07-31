#include "SpaceModel.hpp"

SpaceModel::SpaceModel(
        std::unique_ptr<spacelib::space<2>> &&sp,
        const id_type<SpaceModel> &id,
        QObject *parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_space{std::move(sp)}
{

}
