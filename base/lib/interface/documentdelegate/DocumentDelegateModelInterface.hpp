#pragma once
#include <tools/NamedObject.hpp>

namespace iscore
{
	class DocumentDelegatePresenterInterface;
	class DocumentDelegateModelInterface : public NamedObject
	{
			Q_OBJECT
		public:
			using NamedObject::NamedObject;
			virtual ~DocumentDelegateModelInterface() = default;

			virtual void reset() = 0;
	};
}
