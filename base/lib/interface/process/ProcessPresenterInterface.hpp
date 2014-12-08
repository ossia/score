#pragma once
#include <tools/NamedObject.hpp>

namespace iscore
{
	class ProcessPresenterInterface : public NamedObject
	{
		public:
			using NamedObject::NamedObject;
			virtual ~ProcessPresenterInterface() = default;
			
			virtual int id() const = 0;
	};
}
