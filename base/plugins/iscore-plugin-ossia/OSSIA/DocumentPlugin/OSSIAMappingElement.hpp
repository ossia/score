#pragma once
#include "OSSIAProcessElement.hpp"
#include <State/Address.hpp>
#include <QPointer>
#include <API/Headers/Editor/Value.h>
namespace OSSIA
{
    class Mapper;
    class CurveAbstract;
}

class MappingModel;
class DeviceList;
class OSSIAConstraintElement;


class OSSIAMappingElement final : public OSSIAProcessElement
{
    public:
        OSSIAMappingElement(
                OSSIAConstraintElement& parentConstraint,
                MappingModel& element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const override;
        Process& iscoreProcess() const override;

        void recreate() override;
        void clear() override;

    private:
        std::shared_ptr<OSSIA::CurveAbstract> rebuildCurve();

        OSSIA::Value::Type m_sourceAddressType{OSSIA::Value::Type(-1)};
        OSSIA::Value::Type m_targetAddressType{OSSIA::Value::Type(-1)};

        template<typename T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl();

        template<typename X_T, typename Y_T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl2();

        std::shared_ptr<OSSIA::Mapper> m_ossia_mapping;
        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;

        MappingModel& m_iscore_mapping;

        const DeviceList& m_deviceList;
};
