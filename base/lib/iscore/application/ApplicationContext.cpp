#include "ApplicationContext.hpp"
#include <core/application/Application.hpp>
#include <core/presenter/Presenter.hpp>
/*
iscore::ApplicationContext::ApplicationContext(iscore::Application& appli):
    components{appli.presenter().applicationComponents()},
    documents{appli.presenter().documentManager()}
{

}
*/


iscore::ApplicationContext::ApplicationContext(
        const iscore::ApplicationSettings& app,
        const iscore::ApplicationComponents& c,
        iscore::DocumentManager& d):
    settings{app},
    components{c},
    documents{d}
{

}
