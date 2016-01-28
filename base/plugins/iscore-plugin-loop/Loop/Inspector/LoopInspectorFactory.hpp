#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Loop/LoopProcessMetadata.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <QList>
#include <QString>
#include <memory>

class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
class ConstraintInspectorDelegate;
class ConstraintModel;
}
namespace Loop
{
// TODO Clean this file
class ConstraintInspectorDelegateFactory : public Scenario::ConstraintInspectorDelegateFactory
{
    public:
        virtual ~ConstraintInspectorDelegateFactory();

        std::unique_ptr<Scenario::ConstraintInspectorDelegate> make(
                const Scenario::ConstraintModel& constraint) override;

        bool matches(
                const Scenario::ConstraintModel& constraint) const override;
};

class InspectorFactory final : public ProcessInspectorWidgetDelegateFactory
{
    public:
        InspectorFactory();
        virtual ~InspectorFactory();

    private:
        ProcessInspectorWidgetDelegate* make(
                const Process::ProcessModel&,
                const iscore::DocumentContext&,
                QWidget* parent) const override;
        bool matches(const Process::ProcessModel&) const override;
};
}
