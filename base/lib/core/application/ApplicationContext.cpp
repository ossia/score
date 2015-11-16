#include "ApplicationContext.hpp"
#include <core/application/Application.hpp>
#include <core/presenter/Presenter.hpp>
iscore::ApplicationContext::ApplicationContext(iscore::Application& app):
    app{app},
    components{app.presenter()->applicationComponents()}
{

}
