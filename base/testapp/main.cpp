#include "TestObject.hpp"

#include <core/application/Application.hpp>
int main(int argc, char** argv)
{
    iscore::TestApplication app(argc, argv);

    TestObject obj{app.context()};

    return app.exec();
}
