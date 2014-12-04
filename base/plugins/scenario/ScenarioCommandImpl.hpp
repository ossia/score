#ifndef SCENARIOCOMMANDIMPL_H
#define SCENARIOCOMMANDIMPL_H

#include <QString>
#include <QPointF>
#include <core/presenter/command/SerializableCommand.hpp>

/**
 * @brief The ScenarioCommandImpl class
 *
 * @todo{À finir de déplacer dans commands/CreateEventCommand}
 */
class ScenarioCommandImpl : public iscore::SerializableCommand
{
	public:
		ScenarioCommandImpl(QPointF pos);

		virtual void undo() override;
		virtual void redo() override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		QString m_parentName;
		QPointF m_position;

};

#endif // SCENARIOCOMMANDIMPL_H
