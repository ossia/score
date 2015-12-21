#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Loop/LoopProcessMetadata.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <QList>
#include <QString>
#include <memory>

class ConstraintInspectorDelegate;
class ConstraintModel;
class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

class LoopConstraintInspectorDelegateFactory : public ConstraintInspectorDelegateFactory
{
    public:
        virtual ~LoopConstraintInspectorDelegateFactory();

        std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) override;

        bool matches(const ConstraintModel& constraint) const override;
};

class LoopInspectorFactory final : public ProcessInspectorWidgetDelegateFactory
{
    public:
        LoopInspectorFactory();
        virtual ~LoopInspectorFactory();

    private:
        ProcessInspectorWidgetDelegate* make(
                const Process&,
                const iscore::DocumentContext&,
                QWidget* parent) const override;
        bool matches(const Process&) const override;
};
