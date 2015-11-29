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


class AutomationInspectorFactory final : public InspectorWidgetFactory
{
    public:
        AutomationInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override;

    private:
        const QList<QString>& key_impl() const override;
};
