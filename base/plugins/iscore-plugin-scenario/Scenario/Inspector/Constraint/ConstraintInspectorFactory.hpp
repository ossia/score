#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <qlist.h>

class InspectorWidgetBase;
class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore


class ConstraintInspectorFactory final : public InspectorWidgetFactory
{
    public:
        ConstraintInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

        const QList<QString>& key_impl() const override;
};
