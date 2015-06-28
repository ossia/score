#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <memory>

class ConstraintModel;
namespace OSSIA
{
    class TimeConstraint;
}
class OSSIAConstraintElement : public iscore::ElementPluginModel
{
    public:
        static constexpr iscore::ElementPluginModelType staticPluginId() { return 1; }
        OSSIAConstraintElement(const ConstraintModel* element, QObject* parent);

        std::shared_ptr<OSSIA::TimeConstraint> constraint() const;

        iscore::ElementPluginModelType elementPluginId() const;

        iscore::ElementPluginModel*clone(
                const QObject* element,
                QObject* parent) const override;

        void serialize(const VisitorVariant&) const;

    private:
        std::shared_ptr<OSSIA::TimeConstraint> m_constraint;
};
