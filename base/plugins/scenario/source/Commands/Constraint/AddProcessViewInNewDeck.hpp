#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessSharedModelInterface;
class BoxModel;
class DeckModel;
class ProcessViewModelInterface;

namespace Scenario
{
	namespace Command
	{
		/**
        * @brief The AddProcessViewInNewDeck class
		*/
        class AddProcessViewInNewDeck : public iscore::SerializableCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
                AddProcessViewInNewDeck();
                AddProcessViewInNewDeck(ObjectPath&& constraintPath, QString process);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

				id_type<ProcessSharedModelInterface> processId() const
                { return m_processId; }

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;
                ObjectPath m_processPath;
				QString m_processName;

                id_type<ProcessSharedModelInterface> m_processId{};
                id_type<BoxModel> m_createdBoxId{};
                id_type<DeckModel> m_createdDeckId{};
                id_type<ProcessViewModelInterface> m_createdProcessViewId{};
                id_type<ProcessSharedModelInterface> m_sharedProcessModelId{};
		};
	}
}
