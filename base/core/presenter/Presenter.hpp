#pragma once
#include <core/presenter/CommandQueue.hpp>

namespace iscore
{
	class Model;
	class View;
	class Presenter
	{
		public:
			Presenter(iscore::Model* model, iscore::View* view):
				m_model(model),
				m_view(view)
			{

			}

		private:
			CommandQueue m_commandQueue;
			Model* m_model;
			View* m_view;
	};
}
