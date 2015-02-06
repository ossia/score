#pragma once
#include <boost/optional.hpp>

template<typename tag, typename value_type>
class id
{
	public:

		explicit id() = default;
		explicit id(value_type val) : m_id{val} { }

		friend bool operator==(const id& lhs, const id& rhs)
		{
			return lhs.m_id == rhs.m_id;
		}

		friend bool operator!=(const id& lhs, const id& rhs)
		{
			return lhs.m_id != rhs.m_id;
		}

		friend bool operator<(const id& lhs, const id& rhs)
		{
			return *lhs.val() < *rhs.val();
		}

		explicit operator value_type() const
		{ return m_id; }

		const value_type& val() const
		{ return m_id; }

		void setVal(value_type&& val)
		{ m_id = val; }

	private:
		value_type m_id{};
};

template<typename tag, typename impl>
using optional_tagged_id = id<tag, boost::optional<impl>>;

template<typename tag>
using optional_tagged_int32_id = optional_tagged_id<tag, int32_t>;

template<typename tag>
using id_type = optional_tagged_int32_id<tag>;

template<typename tag>
struct id_hash
{
		std::size_t operator()(const id_type<tag>& id) const
		{
			return std::hash<int32_t>()(*id.val());
		}
};
