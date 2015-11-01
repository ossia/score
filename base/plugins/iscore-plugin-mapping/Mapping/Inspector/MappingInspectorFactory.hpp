#pragma once
#include <QObject>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Mapping/Inspector/MappingInspectorWidget.hpp>
#include <Mapping/MappingModel.hpp>

class MappingInspectorFactory final : public InspectorWidgetFactory
{
    public:
        MappingInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) override
        {
            return new MappingInspectorWidget{
                        safe_cast<const MappingModel&>(sourceElement),
                        doc,
                        parent};
        }


        QList<QString> correspondingObjectsNames() const override
        {
            return {"Mapping"};
        }
};
