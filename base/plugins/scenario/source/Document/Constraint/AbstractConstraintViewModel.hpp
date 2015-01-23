#pragma once
#include <core/tools/IdentifiedObject.hpp>

class ConstraintModel;
class BoxModel;
class AbstractConstraintViewModel : public IdentifiedObject<AbstractConstraintViewModel>
{
		Q_OBJECT

	public:
		AbstractConstraintViewModel(id_type<AbstractConstraintViewModel> id,
									QString name,
									ConstraintModel* model,
									QObject* parent);

		template<typename Impl>
		AbstractConstraintViewModel(Deserializer<Impl>& vis,
									ConstraintModel* model,
									QObject* parent):
			IdentifiedObject<AbstractConstraintViewModel>{vis, parent},
			m_model{model}
		{
			vis.writeTo(*this);
		}

		template<typename Impl>
		AbstractConstraintViewModel(Deserializer<Impl>&& vis,
									ConstraintModel* model,
									QObject* parent):
			IdentifiedObject<AbstractConstraintViewModel>{vis, parent},
			m_model{model}
		{
			vis.writeTo(*this);
		}



		ConstraintModel* model() const
		{ return m_model; }

		bool isBoxShown() const;
		id_type<BoxModel> shownBox() const;

		void hideBox();
		void showBox(id_type<BoxModel> boxId);

	signals:
		void boxRemoved();
		void boxHidden();
		void boxShown(id_type<BoxModel> boxId);

	public slots:
		virtual void on_boxRemoved(id_type<BoxModel> boxId) = 0;


	private:
		// A view model cannot be constructed without a model
		// hence we are safe with a pointer
		ConstraintModel* m_model{};

		id_type<BoxModel> m_shownBox{};
};
