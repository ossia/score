#pragma once

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QList>
class QObject;
class QWidget;
class InspectorWidgetBase;

namespace iscore
{
class Document;
}


/**
     * @brief The InspectorWidgetFactoryInterface class
     *
     * This class has to be registered in the inspector for each plug-in that
     * provides it.
     *
     * When an object in the Document is selected, the pointer to the object is sent
     * to the inspector using the signal-slot mechanism.
     * The factory can then make a widget from the QObject, which can be displayed
     * in the inspector.
     *
     */
class InspectorWidgetFactory : public iscore::GenericFactoryInterface<QList<QString>>
{
        ISCORE_FACTORY_DECL("Inspector")
    public:
        virtual ~InspectorWidgetFactory();

        /**
        * @brief makeWidget Makes a widget for the inspector from an object
        * @param sourceElement Element from which an inspector widget is to be made
        * @return An inspector widget corresponding to the object.
        */
        virtual InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                iscore::Document& doc,
                QWidget* parent) = 0;
};
