#include <core/application/Application.hpp>
#include <android_native_app_glue.h>

#define APPNAME "i-score3"

void android_main(struct android_app* state)
{
    app_dummy(); // Make sure glue isn't stripped
    int dummy_argc = 0;
	char ** dummy_argv = nullptr;
	iscore::Application app(dummy_argc, dummy_argv);
	app.exec();

    ANativeActivity_finish(state->activity);
}

