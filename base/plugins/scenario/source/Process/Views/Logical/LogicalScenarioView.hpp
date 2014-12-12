#pragma once
#include <QGraphicsObject>

/**
 * @brief The LogicalScenarioView class
 *
 * @todo In order to do this we also have to make a specific model ( processviewmodel, not shared) and a presenter, since it is what positions the elements. Here the size is only aesthetical, it is not related to the duration.
 */

class LogicalScenarioView : public QGraphicsObject
{
	Q_OBJECT

	public:

		virtual ~LogicalScenarioView() = default;

	private:

};

