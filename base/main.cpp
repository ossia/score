#include <iostream>
#include <core/plugin/PluginManager.h>
#include <QCoreApplication>

int main(int argc, char **argv) 
{
    QCoreApplication app(argc, argv);
    iscore::PluginManager p;
    p.loadPlugins();
    return 0;
}
