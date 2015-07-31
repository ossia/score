#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <Space/area.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>
class SpaceModel;

// Maps addresses / values to the parameter of an area
class AreaModel : public IdentifiedObject<AreaModel>
{
        Q_OBJECT
    public:
        AreaModel(
                std::unique_ptr<spacelib::area>&& area,
                const SpaceModel& space,
                const id_type<AreaModel>&,
                QObject* parent);

        void setArea(std::unique_ptr<spacelib::area> &&ar);

        const auto& area() const
        { return *m_area; }
        const auto& space() const
        { return m_space; }

    signals:
        void areaChanged();

    private:
        const SpaceModel& m_space;
        std::unique_ptr<spacelib::area> m_area;
        std::map<spacelib::area::parameter, iscore::FullAddressSettings> m_addressMap;
};
