#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>
#include <QMap>

class ConstraintModel;
class TemporalConstraintViewModel;
class BoxModel;
class DeckModel;
class ProcessSharedModelInterface;

class BoxWidget;
class BoxInspectorSection;
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
		explicit ConstraintInspectorWidget (TemporalConstraintViewModel* object, QWidget* parent = 0);

		TemporalConstraintViewModel* viewModel() const;
		ConstraintModel* model() const;

	public slots:
		void reloadDisplayedValues()
		{ updateDisplayedValues(m_currentConstraint); }
		void updateDisplayedValues(TemporalConstraintViewModel* obj);

		// These methods ask for creation and the signals originate from other parts of the inspector
		void createProcess(QString processName);
		void createBox();

		void activeBoxChanged(QString box);
		void minDurationSpinboxChanged(int val);
		void maxDurationSpinboxChanged(int val);
		void rigidCheckboxToggled(bool b);


		// Interface of Constraint
		void on_processCreated(QString processName, int processId);
		void on_processRemoved(int processId);

		void on_boxCreated(int boxId);
		void on_boxRemoved(int boxId);

		// These methods are used to display created things
		void displaySharedProcess(ProcessSharedModelInterface*);
		void setupBox(BoxModel*);

	private:
		TemporalConstraintViewModel* m_currentConstraint{};
		QVector<QMetaObject::Connection> m_connections;

		InspectorSectionWidget* m_durationSection{};

		InspectorSectionWidget* m_processSection{};
		std::vector<InspectorSectionWidget*> m_processesSectionWidgets;

		InspectorSectionWidget* m_boxSection{};
		BoxWidget* m_boxWidget{};
		QMap<int, BoxInspectorSection*> m_boxesSectionWidgets;

		std::vector<QWidget*> m_properties;

};
