#pragma once
#include <core/tools/IdentifiedObject.hpp>

class ConstraintModel;
class AbstractConstraintViewModel : public IdentifiedObject
{
		Q_OBJECT

		friend QDataStream& operator <<(QDataStream& s,
										const AbstractConstraintViewModel& p)
		{
			// It will be deserialized by the constructor.
			s << static_cast<const IdentifiedObject&>(p);
			s << p.m_boxIsPresent
			  << p.m_idOfDisplayedBox;

			p.serialize(s);
			return s;
		}

		friend QDataStream& operator >>(QDataStream& s,
										AbstractConstraintViewModel& p)
		{
			s >> p.m_boxIsPresent
			  >> p.m_idOfDisplayedBox;

			return s;
		}
	public:
		AbstractConstraintViewModel(int id,
									QString name,
									ConstraintModel* model,
									QObject* parent):
			IdentifiedObject{id, name, parent},
			m_model{model}
		{
		}

		AbstractConstraintViewModel(QDataStream& s,
									ConstraintModel* model,
									QObject* parent):
			IdentifiedObject{s, parent},
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

	private:
		// A view model cannot be constructed without a model
		// hence we are safe with a pointer
		ConstraintModel* m_model{};

		bool m_boxIsPresent{};
		int m_idOfDisplayedBox{};
};
