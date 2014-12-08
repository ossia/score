#pragma once
#include <tools/NamedObject.hpp>

class QIdentifiedObject : public QNamedObject
{
	public:
		template<typename... Args>
		QIdentifiedObject(QObject* parent,
						  QString name,
						  int id,
						  Args&&... args):
			QNamedObject{parent,
						 name,
						 std::forward<Args>(args)...},
			m_id{id}
		{
		}

		int id() const
		{
			return m_id;
		}

		void setId(int id)
		{
			m_id = id;
		}

	private:
		int m_id{};
};

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


////////////////////////////////////////////////

template <typename Vector>
int getNextId(Vector& v)
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
