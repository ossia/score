#include "TestObject.hpp"

#include "TestApplication.hpp"
int main(int argc, char** argv)
{
    TestApplication app(argc, argv);

    TestObject obj{app.context()};

    return app.exec();
}
