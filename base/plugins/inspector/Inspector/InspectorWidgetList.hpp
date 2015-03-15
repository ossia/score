#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QVector>

namespace iscore
{
    class FactoryInterface;
}

class InspectorWidgetBase;
class InspectorWidgetFactoryInterface;
class InspectorWidgetList : public NamedObject
{
    public:
        InspectorWidgetList(QObject* parent);

        static InspectorWidgetBase* makeInspectorWidget(QString name, QObject* object);
        void registerFactory(iscore::FactoryInterface* e);

    private:
        QVector<InspectorWidgetFactoryInterface*> m_factories;
};
