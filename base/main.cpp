#include <iostream>
#include <core/application/Application.hpp>

int main(int argc, char **argv)
{
	iscore::Application app(argc, argv);
	return app.exec();
}
