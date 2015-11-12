#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QVector>

namespace iscore
{
    class FactoryInterfaceBase;
}

class InspectorWidgetBase;
class InspectorWidgetFactory;
class InspectorWidgetList : public NamedObject
{
    public:
        explicit InspectorWidgetList(QObject* parent);

        static InspectorWidgetBase* makeInspectorWidget(
                const QString& name,
                const QObject& model,
                QWidget* parent);

        void registerFactory(iscore::FactoryInterfaceBase* e);

    private:
        QVector<InspectorWidgetFactory*> m_factories;
};
