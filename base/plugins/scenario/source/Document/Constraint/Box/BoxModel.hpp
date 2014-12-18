#pragma once
#include <tools/IdentifiedObject.hpp>

#include <vector>

class ConstraintModel;
class StoreyModel;

class BoxModel : public IdentifiedObject
{
	Q_OBJECT

	public:
		friend QDataStream& operator << (QDataStream&, const BoxModel&);
		BoxModel(int id, ConstraintModel* parent);
		BoxModel(QDataStream&, ConstraintModel* parent);

		virtual ~BoxModel() = default;

		int createStorey(int newStoreyId);
		int createStorey(QDataStream& s);
		void deleteStorey(int storeyId);
		void changeStoreyOrder(int storeyId, int position);

		StoreyModel* storey(int storeyId) const;

		// Devrait peut-être aller dans une Command à la place ?
		void duplicateStorey();

		const std::vector<StoreyModel*>& storeys() const
		{ return m_storeys; }

	signals:
		void storeyCreated(int id);
		void storeyDeleted(int id);
		void storeyOrderChanged(int storeyId);

		void on_deleteSharedProcessModel(int processId);

	private:
		int createStorey_impl(StoreyModel* m);
		std::vector<StoreyModel*> m_storeys;
};

