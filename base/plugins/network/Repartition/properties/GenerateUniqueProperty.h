#pragma once
#include <functional>

#define GenerateUniqueProperty(Property, Type) \
	class has ## Property \
	{ \
		public: \
			has ## Property(Type val): \
				_val(val) \
			{ } \
			\
			virtual ~has ## Property() = default;\
			\
			Type get ## Property() const \
			{ \
				return _val; \
			} \
			\
			void set ## Property(const Type& val) \
			{ \
				_val = val; \
			} \
			\
			static std::function<bool(const has ## Property&)> hasSame(const Type& val) \
			{ \
				return [&val] (const has ## Property& p) \
				{ \
					return p._val == val; \
				}; \
			} \
			\
		private: \
			Type _val; \
	};
