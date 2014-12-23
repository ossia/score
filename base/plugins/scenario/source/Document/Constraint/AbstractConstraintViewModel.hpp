#pragma once
#include <core/tools/IdentifiedObject.hpp>

class ConstraintModel;
class AbstractConstraintViewModel : public IdentifiedObject
{
		Q_OBJECT

		friend QDataStream& operator <<(QDataStream& s,
										const AbstractConstraintViewModel& p);

		friend QDataStream& operator >>(QDataStream& s,
										AbstractConstraintViewModel& p);

	public:
		ConstraintModel* model() const
		{ return m_model; }

		bool isBoxShown() const;
		int shownBox() const;

		void hideBox();
		void showBox(int boxId);

	signals:
		void boxCreated(int boxId);
		void boxRemoved(int boxId);
		void boxHidden();
		void boxShown(int boxId);

	protected:
		virtual void serialize(QDataStream&) const = 0;
		AbstractConstraintViewModel(int id,
									QString name,
									ConstraintModel* model,
									QObject* parent);

		AbstractConstraintViewModel(QDataStream& s,
									ConstraintModel* model,
									QObject* parent);

	private:
		// A view model cannot be constructed without a model
		// hence we are safe with a pointer
		ConstraintModel* m_model{};

		bool m_boxIsShown{};
		int m_idOfShownBox{};
};
