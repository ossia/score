#pragma once
namespace iscore
{
	// Base class for factories of elements whose type is not part of the base application.
	class FactoryInterface
	{
		public:
			virtual ~FactoryInterface() = default;
	};
}
