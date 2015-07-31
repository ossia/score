#include "AreaModel.hpp"
AreaModel::AreaModel(
        std::unique_ptr<spacelib::area>&& area,
        const SpaceModel& space,
        const id_type<AreaModel> & id,
        QObject *parent):
    IdentifiedObject{id, staticMetaObject.className(), parent},
    m_space{space},
    m_area{std::move(area)}
{
    for(const auto& parameter : m_area->parameters())
    {
        m_addressMap.insert(GiNaC::ex_to<GiNaC::symbol>(parameter.first), {false, iscore::FullAddressSettings{}});
    }
}

void AreaModel::setArea(std::unique_ptr<spacelib::area> &&ar)
{
    m_area = std::move(ar);
}

void AreaModel::mapAddressToParameter(const QString& str, const iscore::FullAddressSettings& addr)
{
    m_addressMap[parameterFromString(str)] = {false, addr};
}

void AreaModel::mapValueToParameter(const QString& str, const iscore::Value&& val)
{
    auto param = parameterFromString(str);
    m_addressMap[param].first = true;
    m_addressMap[param].second.value = val;
}

spacelib::area::parameter AreaModel::parameterFromString(const QString& str)
{
    auto it = std::find_if(m_addressMap.keys().begin(),
                           m_addressMap.keys().end(),
                           [&] (const spacelib::area::parameter& param)
    { return param.get_name() == str.toStdString(); });

    Q_ASSERT(it != m_addressMap.keys().end());
    return *it;
}
