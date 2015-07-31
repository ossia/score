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
        void setSpaceMapping(const GiNaC::exmap& mapping);

        const auto& area() const
        { return *m_area; }
        spacelib::projected_area projectedArea() const;
        spacelib::valued_area valuedArea() const;

        const auto& space() const
        { return m_space; }
        const auto& spaceMapping() const
        { return m_spaceMapping; }

        void mapAddressToParameter(const QString& str,
                                   const iscore::FullAddressSettings& addr);

        void mapValueToParameter(const QString& str,
                                 const iscore::Value&& val);

    signals:
        void areaChanged();

    private:
        const SpaceModel& m_space;
        std::unique_ptr<spacelib::area> m_area;

        GiNaC::exmap m_spaceMapping;

        // bool is true if we use only the value, false if we
        // use the whole address
        QMap<QString,
             QPair<GiNaC::symbol, QPair<bool, iscore::FullAddressSettings>>
        > m_addressMap;
};
