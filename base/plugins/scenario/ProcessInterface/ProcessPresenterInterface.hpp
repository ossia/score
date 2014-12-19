#pragma once
#include <tools/NamedObject.hpp>

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

		virtual int viewModelId() const = 0;
		virtual int modelId() const = 0;

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);
};
