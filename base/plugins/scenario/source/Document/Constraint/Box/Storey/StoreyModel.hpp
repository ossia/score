#pragma once
#include <tools/IdentifiedObject.hpp>

#include <vector>

class BoxModel;
class ConstraintModel;

namespace iscore
{
}

class ProcessViewModelInterface;
class StoreyModel : public IdentifiedObject
{
	Q_OBJECT

		Q_PROPERTY(int height
				   READ height
				   WRITE setHeight
				   NOTIFY heightChanged)

	public:
		friend QDataStream& operator << (QDataStream& , const StoreyModel& );
		friend QDataStream& operator >> (QDataStream& , StoreyModel& );

		StoreyModel(QDataStream& s, BoxModel* parent);
		StoreyModel(int position, int id, BoxModel* parent);
		virtual ~StoreyModel() = default;

		int createProcessViewModel(int sharedProcessId, int newProcessViewModelId);
		int createProcessViewModel(QDataStream& s,int sharedProcessId);
		void deleteProcessViewModel(int processViewModelId);

		/**
		 * @brief selectForEdition
		 * @param processViewId
		 *
		 * A process is selected for edition when it is
		 * the edited process when the interface is clicked.
		 */
		void selectForEdition(int processViewId);

		const std::vector<ProcessViewModelInterface*>& processViewModels() const;
		ProcessViewModelInterface* processViewModel(int processViewModelId) const;

		/**
		 * @brief parentConstraint
		 * @return the constraint this storey is part of.
		 */
		ConstraintModel* parentConstraint() const;

		int height() const;
		int position() const;

	signals:
		void processViewModelCreated(int processViewModelId);
		void processViewModelDeleted(int processViewModelId);
		void processViewModelSelected(int processViewModelId);

		void heightChanged(int arg);
		void positionChanged(int arg);

		void storeyBecomesEmpty(int id);

	public slots:
		void on_deleteSharedProcessModel(int sharedProcessId);

		void setHeight(int arg);
		void setPosition(int arg);

	private:
		int m_editedProcessId{};
		std::vector<ProcessViewModelInterface*> m_processViewModels;

		int m_height{200};
		int m_position{};
};

