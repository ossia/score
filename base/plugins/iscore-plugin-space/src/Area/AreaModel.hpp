#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <Space/area.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <src/Area/AreaFactory.hpp>
#include <src/SpaceContext.hpp>
#include <Process/ModelMetadata.hpp>
Q_DECLARE_METATYPE(GiNaC::exmap)
class SpaceModel;
class QGraphicsItem;


namespace Space
{
// in the end, isn't an area the same thing as a domain???
// Maps addresses / values to the parameter of an area
class AreaPresenter;
class AreaModel : public IdentifiedObject<AreaModel>
{
        Q_OBJECT
    public:
        ModelMetadata metadata;

        // The value is used as default value if the address is invalid.
        using ParameterMap = QMap<QString, QPair<GiNaC::symbol, Device::FullAddressSettings>>;
        virtual const AreaFactoryKey& factoryKey() const = 0;
        virtual QString prettyName() const = 0;
        virtual int type() const = 0;

        AreaModel(
                std::unique_ptr<spacelib::area>&& area,
                const Space::AreaContext& space,
                const Id<AreaModel>&,
                QObject* parent);


        const spacelib::area& area() const
        { return *m_area; }
        spacelib::projected_area projectedArea() const;
        spacelib::valued_area valuedArea(const GiNaC::exmap& vals) const;

        const SpaceModel& space() const
        { return m_context.space; }

        void setSpaceMapping(const GiNaC::exmap& mapping);
        const GiNaC::exmap& spaceMapping() const
        { return m_spaceMap; }

        void setParameterMapping(const ParameterMap& mapping);
        const ParameterMap& parameterMapping() const
        { return m_parameterMap; }

        void setCurrentMapping(const GiNaC::exmap& map);
        const GiNaC::exmap& currentMapping() const
        { return m_currentParameterMap;}

        QString toString() const;

    signals:
        void areaChanged(GiNaC::exmap);

    private:
        const Space::AreaContext& m_context;
        std::unique_ptr<spacelib::area> m_area;

        // Maps a variable from m_area to a variable from m_space.
        GiNaC::exmap m_spaceMap;

        ParameterMap m_parameterMap; // General values of the model.
        GiNaC::exmap m_currentParameterMap; // Current values used for display / execution.
};
}

Q_DECLARE_METATYPE(Id<Space::AreaModel>)
