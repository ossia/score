#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>

class ConstraintModel;
class BoxModel;
class StoreyModel;
class ProcessSharedModelInterface;

class BoxWidget;
class QFormLayout;

/*!
 * \brief The ConstraintInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an Constraint (Timebox) element.
 */
class ConstraintInspectorWidget : public InspectorWidgetBase
{
		Q_OBJECT
	public:
		explicit ConstraintInspectorWidget (ConstraintModel* object, QWidget* parent = 0);

		ConstraintModel* model() const
		{ return m_currentConstraint; }

	public slots:
		void updateDisplayedValues (ConstraintModel* obj);

		// These methods ask for creation
		void createProcess(QString processName);
		void createBox();
		void createDeck(int box);

		// Interface of Constraint
		void on_processCreated(QString processName, int processId);
		void on_processRemoved(int processId);

		void on_boxCreated(int viewId);
		void on_boxRemoved(int viewId);

		// Interface of Box

		// Interface of Deck

		// Abstract interface of SharedProcessModel

		// Abstract interface of ProcessViewModel



		// These methods are used to display created things
		void displaySharedProcess(ProcessSharedModelInterface*);
		void displayBox(BoxModel*);
		void displayDeck(StoreyModel*);

	private:
		ConstraintModel* m_currentConstraint{};
		QVector<QMetaObject::Connection> m_connections;

		InspectorSectionWidget* m_processSection{};
		std::vector<InspectorSectionWidget*> m_processesSectionWidgets;

		InspectorSectionWidget* m_boxSection{};
		BoxWidget* m_boxWidget{};
		std::vector<InspectorSectionWidget*> m_boxesSectionWidgets;

		InspectorSectionWidget* m_deckSection{};
		//DeckWidget* m_deckWidget{};
		std::vector<InspectorSectionWidget*> m_decksSectionWidgets;

		std::vector<QWidget*> m_properties;

};
