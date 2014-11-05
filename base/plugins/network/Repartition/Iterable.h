#pragma once
#include <string>
#include <algorithm>
#include <vector>
#include <set>

#include <iostream>
#include <boost/iterator/indirect_iterator.hpp>

template<typename T>
using SimpleVec = std::vector<T, std::allocator<T>>;

template <typename T,
		  template <typename> class Container = SimpleVec,
		  class String = std::string>
class Iterable
{

		friend boost::indirect_iterator<typename Container<std::unique_ptr<T>>::iterator>
			begin(Iterable& i)
		{
			return boost::indirect_iterator<typename Container<std::unique_ptr<T>>::iterator>(begin(i._c));
		}
		
		friend boost::indirect_iterator<typename Container<std::unique_ptr<T>>::iterator>
			end(Iterable& i)
		{
			return boost::indirect_iterator<typename Container<std::unique_ptr<T>>::iterator>(end(i._c));
		}
		
		friend boost::indirect_iterator<typename Container<std::unique_ptr<T>>::iterator>
			cbegin(const Iterable& i)
		{
			return boost::indirect_iterator<typename Container<std::unique_ptr<T>>::iterator>(cbegin(i._c));
		}
		
		friend boost::indirect_iterator<typename Container<std::unique_ptr<T>>::iterator>
			cend(const Iterable& i)
		{
			return boost::indirect_iterator<typename Container<std::unique_ptr<T>>::iterator>(cend(i._c));
		}

	public:
		Iterable() = default;
		Iterable(Iterable<T, Container, String>&&) = default;
		Iterable<T, Container, String>& operator=(Iterable<T, Container, String>&&) = default;

		Iterable(const Iterable<T, Container, String>&) = default;
		Iterable<T, Container, String>& operator=(const Iterable<T, Container, String>&) = default;
		virtual ~Iterable() = default;

		template<typename... K>
		void performUniquenessCheck(K&&... args)
		{
			auto check = T::hasSame(args...);

			for(auto& elt : _c)
			{
				if(check(*elt.get()))
					throw "Element already exists";
			}
		}

		template<typename... K>
		T& create(K&&... args)
		{
			_c.emplace_back(new T(std::forward<K>(args)...));
			return *_c.back().get();
		}

		template<typename... K>
		void remove(K&&... args)
		{
			auto check = T::hasSame(args...);

			for(auto it = begin(_c); it != end(_c); ++it)
			{
				if(check(*it->get()))
				{
					_c.erase(it);
					return;
				}
			}
		}

		template<typename... K>
		T& operator()(const K&... args)
		{
			auto check = T::hasSame(args...);

			for(auto& elt : _c)
			{
				auto& ref = *elt.get();
				if(check(ref))
					return ref;
			}

			throw "Not found. Did you call has() ?";
		}

		template<typename... K>
		std::vector<std::reference_wrapper<T> > getFamily(const K&... args)
		{
			std::vector<std::reference_wrapper<T> > vect;
			auto cond = T::hasSame(args...);

			for(auto& elt : _c)
			{
				if(cond(*elt))
				{
					vect.emplace_back(*elt.get());
				}
			}

			return vect;
		}

		template<typename... K>
		bool has(K&&... args) const
		{
			auto check = T::hasSame(args...);

			for(auto& elt : _c)
			{
				if(check(*elt.get()))
					return true;
			}

			return false;
		}


	private:
		Container<std::unique_ptr<T>> _c; // Container ex. : std::vector<T>
};

