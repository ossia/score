#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QVector>

namespace iscore
{
    class FactoryInterface;
}

class InspectorWidgetBase;
class InspectorWidgetFactory;
class InspectorWidgetList : public NamedObject
{
    public:
        InspectorWidgetList(QObject* parent);

        static InspectorWidgetBase* makeInspectorWidget(const QString& name,
                                                        const QObject* model,
                                                        QWidget* parent);
        void registerFactory(iscore::FactoryInterface* e);

    private:
        QVector<InspectorWidgetFactory*> m_factories;
};
