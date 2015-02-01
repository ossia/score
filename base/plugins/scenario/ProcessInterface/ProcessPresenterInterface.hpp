#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifier.hpp>

class ProcessSharedModelInterface;
class ProcessViewModelInterface;
namespace iscore
{
	class SerializableCommand;
}
class ProcessPresenterInterface : public NamedObject
{
		Q_OBJECT
	public:
		using NamedObject::NamedObject;
		virtual ~ProcessPresenterInterface() = default;

		virtual void putToFront() = 0;
		virtual void putBack() = 0;

		virtual void on_horizontalZoomChanged(int) = 0;

		virtual id_type<ProcessViewModelInterface> viewModelId() const = 0;
		virtual id_type<ProcessSharedModelInterface> modelId() const = 0;

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);
};
