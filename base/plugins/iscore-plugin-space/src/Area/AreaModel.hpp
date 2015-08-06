#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <Space/area.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>
class SpaceModel;

// in the end, isn't an area the same thing as a domain???
// Maps addresses / values to the parameter of an area
class AreaModel : public IdentifiedObject<AreaModel>
{
        Q_OBJECT
    public:
        // bool is true if we use only the value, false if we
        // use the whole address
        using ParameterMap = QMap<QString, QPair<GiNaC::symbol, QPair<bool, iscore::FullAddressSettings>>>;

        AreaModel(
                std::unique_ptr<spacelib::area>&& area,
                const SpaceModel& space,
                const id_type<AreaModel>&,
                QObject* parent);


        const spacelib::area& area() const
        { return *m_area; }
        spacelib::projected_area projectedArea() const;
        spacelib::valued_area valuedArea() const;

        const SpaceModel& space() const
        { return m_space; }

        void setSpaceMapping(const GiNaC::exmap& mapping);
        const GiNaC::exmap& spaceMapping() const
        { return m_spaceMap; }

        void setParameterMapping(const ParameterMap& mapping);
        const ParameterMap& parameterMapping() const
        { return m_parameterMap; }

        void mapAddressToParameter(const QString& str,
                                   const iscore::FullAddressSettings& addr);

        void mapValueToParameter(const QString& str,
                                 const iscore::Value&& val);

        QString toString() const;

    signals:
        void areaChanged();

    private:
        const SpaceModel& m_space;
        std::unique_ptr<spacelib::area> m_area;

        // Maps a variable from m_area to a variable from m_space.
        GiNaC::exmap m_spaceMap;

        ParameterMap m_parameterMap;
};

Q_DECLARE_METATYPE(id_type<AreaModel>)
