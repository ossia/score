#pragma once
#include <tools/NamedObject.hpp>

namespace iscore
{
	class ProcessPresenterInterface : public QNamedObject
	{
		public:
			using QNamedObject::QNamedObject;
			virtual ~ProcessPresenterInterface() = default;
			
			virtual int id() const = 0;
	};
}
