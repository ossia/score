#pragma once

#include <ProcessInterface/ProcessViewModelInterface.hpp>

class AutomationModel;
class AutomationViewModel : public ProcessViewModelInterface
{
	public:
		AutomationViewModel(AutomationModel* model,
							id_type<ProcessViewModelInterface> id,
							QObject* parent);

		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const;

		AutomationModel* model();


	private:
		AutomationModel* m_model{};
};
