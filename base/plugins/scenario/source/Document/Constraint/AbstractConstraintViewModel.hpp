#pragma once
#include <core/tools/IdentifiedObject.hpp>

class ConstraintModel;
class AbstractConstraintViewModel : public IdentifiedObject
{
		Q_OBJECT

	public:
		AbstractConstraintViewModel(int id,
									QString name,
									ConstraintModel* model,
									QObject* parent);

		template<typename Impl>
		AbstractConstraintViewModel(Deserializer<Impl>& vis,
									ConstraintModel* model,
									QObject* parent):
			IdentifiedObject{vis, parent},
			m_model{model}
		{
			vis.writeTo(*this);
		}


		ConstraintModel* model() const
		{ return m_model; }

		bool isBoxShown() const;
		int shownBox() const;

		void hideBox();
		void showBox(int boxId);

	signals:
		void boxRemoved();
		void boxHidden();
		void boxShown(int boxId);

	public slots:
		virtual void on_boxRemoved(int boxId) = 0;


	private:
		// A view model cannot be constructed without a model
		// hence we are safe with a pointer
		ConstraintModel* m_model{};

		// TODO use settable identifier instead
		bool m_boxIsShown{};
		int m_idOfShownBox{};
};
