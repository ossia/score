#pragma once

template<typename Impl>
class Visitor
{
	public:
		template<typename T>
		void visit(T&);
};

