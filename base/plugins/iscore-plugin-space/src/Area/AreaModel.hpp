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

        void mapAddressToParameter(const QString& str,
                                   const iscore::FullAddressSettings& addr);

        void mapValueToParameter(const QString& str,
                                 const iscore::Value&& val);

    signals:
        void areaChanged();

    private:
        spacelib::area::parameter parameterFromString(const QString& str);
        const SpaceModel& m_space;
        std::unique_ptr<spacelib::area> m_area;

        // bool is true if we use only the value, false if we
        // use the whole address
        QMap<spacelib::area::parameter,
             QPair<bool, iscore::FullAddressSettings>
        > m_addressMap;
};
