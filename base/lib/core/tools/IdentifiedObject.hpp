#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifier.hpp>

// This should maybe be a mixin ?
class IdentifiedObject : public NamedObject
{

	public:
		template<typename... Args>
		IdentifiedObject(SettableIdentifier id,
						 Args&&... args):
			NamedObject{std::forward<Args>(args)...},
			m_id{id}
		{
		}

		template<typename ReaderImpl,typename... Args>
		IdentifiedObject(Deserializer<ReaderImpl>& v, Args&&... args):
			NamedObject{v, std::forward<Args>(args)...}
		{
			v.visit(*this);
		}

		const SettableIdentifier& id() const
		{
			return m_id;
		}

		void setId(SettableIdentifier&& id)
		{
			m_id = id;
		}

	private:
		SettableIdentifier m_id{};
};

////////////////////////////////////////////////
///
///Functions that operate on collections of identified objects.
///
////////////////////////////////////////////////
#include <typeinfo>
template<typename Container>
typename Container::value_type findById(const Container& c, int id)
{
	auto it = std::find_if(std::begin(c),
						   std::end(c),
						   [&id] (typename Container::value_type model)
							{
							  return model->id() == id;
							});

	if(it != std::end(c))
		return *it;

	throw std::runtime_error(QString("findById : id %1 not found in vector of %2").arg(id).arg(typeid(c).name()).toLatin1().constData());
}


inline int32_t getNextId()
{
	using namespace std;
	static random_device rd;
	static mt19937 gen(rd());
	static uniform_int_distribution<int32_t>
			dist(numeric_limits<int32_t>::min(),
				 numeric_limits<int32_t>::max());

	return dist(gen);
}

// TODO replace by a function that does random generation and checks
// if it exists in the vector
template <typename Vector>
int getNextId(const Vector& v)
{
	using namespace std;
	vector<int> ids(v.size()); // Map reduce
	transform(begin(v),
			  end(v),
			  begin(ids),
			  [] (typename Vector::value_type elt) { return elt->id(); });

	int id{};
	do {
		id = getNextId();
	} while(find(begin(ids),
				 end(ids),
				 id) != end(ids));

	return id;
}

#include "utilsCPP11.hpp"
template <typename Vector>
void removeById(Vector& c, int id)
{
	vec_erase_remove_if(c,
						[&id] (typename Vector::value_type model)
						{
							bool to_delete = model->id() == id;
							if(to_delete) delete model;
							return to_delete;
						} );
}

template<typename hasId>
void removeFromVectorWithId(std::vector<hasId*>& v, int id)
{
	auto it = std::find_if(std::begin(v),
						   std::end(v),
						   [id] (hasId const * elt)
				{
					return elt->id() == id;
				});

	if(it != std::end(v))
	{
		delete *it;
		v.erase(it);
	}
}
