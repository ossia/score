#pragma once
#include <core/tools/IdentifiedObject.hpp>

class ConstraintModel;
class ConstraintViewModelInterface : public IdentifiedObject
{
		Q_OBJECT

		friend QDataStream& operator <<(QDataStream& s, const ConstraintViewModelInterface& p)
		{
			// It will be deserialized by the constructor.
			s << static_cast<const IdentifiedObject&>(p);

			p.serialize(s);
			return s;
		}

		friend QDataStream& operator >>(QDataStream& s, ConstraintViewModelInterface& p)
		{
			p.deserialize(s);
			return s;
		}

	public:
		ConstraintViewModelInterface(int id,
									 QString name,
									 ConstraintModel* model,
									 QObject* parent):
			IdentifiedObject{id, name, parent},
			m_model{model}
		{
		}

		ConstraintViewModelInterface(QDataStream& s,
									 QString name,
									 ConstraintModel* model,
									 QObject* parent):
			IdentifiedObject{s, name, parent},
			m_model{model}
		{
			s >> *this;
		}

		ConstraintModel* model() const
		{ return m_model; }

	signals:
		void boxCreated(int boxId);
		void boxRemoved(int boxId);

	protected:
		virtual void serialize(QDataStream&) const = 0;
		virtual void deserialize(QDataStream&) = 0;

	private:
		// A view model cannot exist without a model hence we are safe with a pointer
		ConstraintModel* m_model{};
};
