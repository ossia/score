#pragma once
#include <tools/IdentifiedObject.hpp>

#include <vector>

class IntervalContentModel;
class IntervalModel;

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

		StoreyModel(QDataStream& s, IntervalContentModel* parent);
		StoreyModel(int id, IntervalContentModel* parent);
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

		const std::vector<ProcessViewModelInterface*>&
		processViewModels() const
		{
			return m_processViewModels;
		}

		ProcessViewModelInterface* processViewModel(int processViewModelId) const;

		/**
		 * @brief parentInterval
		 * @return the interval this storey is part of.
		 */
		IntervalModel* parentInterval() const;

		int height() const
		{
			return m_height;
		}

	signals:
		void processViewModelCreated(int processViewModelId);
		void processViewModelDeleted(int processViewModelId);

		void processViewModelSelected(int processViewModelId);

		void heightChanged(int arg);

	public slots:
		void on_deleteSharedProcessModel(int sharedProcessId);

		void setHeight(int arg)
		{
			if (m_height != arg) {
				m_height = arg;
				emit heightChanged(arg);
			}
		}

	private:

		int m_editedProcessId{};
		std::vector<ProcessViewModelInterface*> m_processViewModels;

		int m_height{500};
};

