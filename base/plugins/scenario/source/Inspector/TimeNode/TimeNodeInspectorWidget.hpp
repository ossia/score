#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>
class TimeNodeModel;

class QFormLayout;
class MetadataWidget;

/*!
 * \brief The TimeNodeInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an TimeNode (Timebox) element.
 */
class TimeNodeInspectorWidget : public InspectorWidgetBase
{
		Q_OBJECT
	public:
		explicit TimeNodeInspectorWidget (TimeNodeModel* object, QWidget* parent = 0);

	signals:

	public slots:
		void updateDisplayedValues (TimeNodeModel* obj);

        void updateInspector();

        void on_scriptingNameChanged(QString);
        void on_labelChanged(QString);
        void on_commentsChanged(QString);
        void on_colorChanged(QColor);

	private:
        QVector<QWidget*> m_properties;
        std::vector<QPushButton*> m_events;

		TimeNodeModel* m_timeNodeModel{};

        InspectorSectionWidget* m_eventList{};
        QLabel* m_date{};

        MetadataWidget* m_metadata{};
};
