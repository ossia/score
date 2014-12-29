#pragma once

#include <interface/customfactory/FactoryInterface.hpp>
class InspectorWidgetBase;

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
class InspectorWidgetFactoryInterface : public iscore::FactoryInterface
{
	public:
		/**
		* @brief correspondingObjectName
		* @return the name of the object for which this inspector is meant to generate a widget.
		*/
		virtual QString correspondingObjectName() const = 0;

		/**
		* @brief makeWidget Makes a widget for the inspector from an object
		* @param sourceElement Element from which an inspector widget is to be made
		* @return An inspector widget corresponding to the object.
		*/
		virtual InspectorWidgetBase* makeWidget(QObject* sourceElement) = 0;

		/**
		* @brief makeWidget Makes a widget for the inspector from a list of objects
		* @param sourceElements List of elements
		* @return An inspector widget corresponding to the objects
		*/
		virtual InspectorWidgetBase* makeWidget(QList<QObject*> sourceElements) = 0;
};
