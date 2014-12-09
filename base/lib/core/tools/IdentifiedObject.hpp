#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifier.hpp>

class IdentifiedObject : public NamedObject
{
	public:
		// @todo argument that takes a QDataStream&;
		template<typename... Args>
		IdentifiedObject(QObject* parent,
						  QString name,
						  SettableIdentifier id,
						  Args&&... args):
			NamedObject{parent,
						 name,
						 std::forward<Args>(args)...},
			m_id{id}
		{
		}

		const SettableIdentifier& id() const
		{
			return m_id;
		}

		void setId(SettableIdentifier id)
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

	return nullptr;
}

template <typename Vector>
int getNextId(const Vector& v)
{
	if(v.size() == 0)
	{
		return 0;
	}

	std::vector<int> ids(v.size()); // Map reduce
	std::transform(std::begin(v),
				   std::end(v),
				   std::begin(ids),
				   [] (typename Vector::value_type elt) { return elt->id(); });

	return *(std::max_element(std::begin(ids),
							  std::end(ids))) + 1;
}

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
