#include "ApplicationContext.hpp"


iscore::ApplicationContext::ApplicationContext(
        const iscore::ApplicationSettings& app,
        const iscore::ApplicationComponents& c,
        iscore::DocumentManager& d):
    settings{app},
    components{c},
    documents{d}
{

}
