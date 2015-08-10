#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <Space/area.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>
class SpaceModel;
class QGraphicsItem;

// in the end, isn't an area the same thing as a domain???
// Maps addresses / values to the parameter of an area
class AreaPresenter;
class AreaModel : public IdentifiedObject<AreaModel>
{
        Q_OBJECT
    public:
        // The value is used as default value if the address is invalid.
        using ParameterMap = QMap<QString, QPair<GiNaC::symbol, iscore::FullAddressSettings>>;

        static constexpr int static_type() { return 0; }
        static QString pretty_name() { return tr("Generic"); }
        virtual int type() const { return 0; }

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

        QString toString() const;

        // Args : parent of the view, parent of the presenter
        virtual AreaPresenter* makePresenter(QGraphicsItem* , QObject*) const;
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
