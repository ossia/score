#pragma once

//make_unique is not available in C++11, only in C++14
//cf http://stackoverflow.com/questions/7038357/make-unique-and-perfect-forwarding
//warning: this implementation will not work with VS2012 and earlier
// cf http://stackoverflow.com/questions/12547983/is-there-a-way-to-write-make-unique-in-vs2012
#include <memory>
#if (__cplusplus < 201300L)
namespace std {
  template<typename T, typename... Args>
  std::unique_ptr<T> make_unique(Args&&... args)
  {
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
  }
}
#endif

#include <algorithm>
template <typename Vector, typename Functor>
void vec_erase_remove_if(Vector& v, Functor&& f )
{
	v.erase(std::remove_if(std::begin(v), std::end(v), f), std::end(v));
}

#include <QByteArray>
#include <QDataStream>
template<typename InputVector, typename OutputVector>
void serializeVectorOfPointers(const InputVector& in, OutputVector& out)
{
	for(const auto& elt : in)
	{
		QByteArray arr;

		QDataStream s(&arr, QIODevice::WriteOnly);
		s.setVersion(QDataStream::Qt_5_3);
		s << *elt;

		out.push_back(arr);
	}
}


#include <tuple>
inline QDataStream& operator<<(QDataStream& s, const std::tuple<int,int,int>& tuple)
{
	s << std::get<0>(tuple) << std::get<1>(tuple) << std::get<2>(tuple);
	return s;
}
inline QDataStream& operator>>(QDataStream& s, std::tuple<int,int,int>& tuple)
{
	s >> std::get<0>(tuple) >> std::get<1>(tuple) >> std::get<2>(tuple);
	return s;
}
