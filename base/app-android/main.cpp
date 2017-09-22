// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <core/application/Application.hpp>
//#include <android_native_app_glue.h>

#define APPNAME "score3"
#if defined(SCORE_STATIC_PLUGINS)
Q_IMPORT_PLUGIN(score_plugin_scenario)
Q_IMPORT_PLUGIN(InspectorPlugin)
Q_IMPORT_PLUGIN(DeviceExplorerPlugin)
Q_IMPORT_PLUGIN(PluginSettingsPlugin)  // static plug-ins should not be displayed.
#endif
//void android_main(struct android_app* state)
int main()
{
    //app_dummy(); // Make sure glue isn't stripped
    int dummy_argc = 0;
    char** dummy_argv = nullptr;
    score::Application app(dummy_argc, dummy_argv);
    app.exec();

    //ANativeActivity_finish(state->activity);
}

