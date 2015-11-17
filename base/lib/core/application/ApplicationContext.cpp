#include "ApplicationContext.hpp"
#include <core/application/Application.hpp>
#include <core/presenter/Presenter.hpp>
iscore::ApplicationContext::ApplicationContext(iscore::Application& appli):
    app{appli},
    components{app.presenter().applicationComponents()}
{

}
