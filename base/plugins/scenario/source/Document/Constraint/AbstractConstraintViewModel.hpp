#pragma once
#include <core/tools/IdentifiedObject.hpp>

class ConstraintModel;
class AbstractConstraintViewModel : public IdentifiedObjectAlternative<AbstractConstraintViewModel>
{
		Q_OBJECT

	public:
		using id_type = ::id_type<AbstractConstraintViewModel>;

		AbstractConstraintViewModel(id_type id,
									QString name,
									ConstraintModel* model,
									QObject* parent);

		template<typename Impl>
		AbstractConstraintViewModel(Deserializer<Impl>& vis,
									ConstraintModel* model,
									QObject* parent):
			IdentifiedObjectAlternative<AbstractConstraintViewModel>{vis, parent},
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
