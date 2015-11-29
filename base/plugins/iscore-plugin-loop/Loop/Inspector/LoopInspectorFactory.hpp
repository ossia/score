#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Loop/LoopProcessMetadata.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <qlist.h>
#include <qstring.h>
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

class LoopInspectorFactory final : public InspectorWidgetFactory
{
    public:
        LoopInspectorFactory();
        virtual ~LoopInspectorFactory();

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        const QList<QString>& key_impl() const override
        {
            static const QList<QString> list{LoopProcessMetadata::processObjectName()};
            return list;
        }
};
