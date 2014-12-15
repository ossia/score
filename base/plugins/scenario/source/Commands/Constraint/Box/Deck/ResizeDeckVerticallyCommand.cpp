#include "ResizeDeckVerticallyCommand.hpp"

#include "Document/Constraint/Box/Storey/StoreyModel.hpp"


ResizeDeckVerticallyCommand::ResizeDeckVerticallyCommand(ObjectPath&& deckPath, int newSize):
	iscore::SerializableCommand{"ScenarioControl",
								"ResizeDeckVerticallyCommand",
								"Resize Deck"},
	m_path{deckPath},
	m_newSize{newSize}
{
	auto deck = static_cast<StoreyModel*>(m_path.find());
	m_originalSize = deck->height();

	qDebug()<<Q_FUNC_INFO << m_originalSize << m_newSize;
}

void ResizeDeckVerticallyCommand::undo()
{
	auto deck = static_cast<StoreyModel*>(m_path.find());
	deck->setHeight(m_originalSize);
}

void ResizeDeckVerticallyCommand::redo()
{
	auto deck = static_cast<StoreyModel*>(m_path.find());
	deck->setHeight(m_newSize);
}

int ResizeDeckVerticallyCommand::id() const
{
	return 1;
}

bool ResizeDeckVerticallyCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void ResizeDeckVerticallyCommand::serializeImpl(QDataStream&)
{

}

void ResizeDeckVerticallyCommand::deserializeImpl(QDataStream&)
{

}
