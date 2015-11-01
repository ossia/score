#include "ScenarioStyle.hpp"

ScenarioStyle& ScenarioStyle::instance()
{
    static ScenarioStyle s;
    return s;
}
