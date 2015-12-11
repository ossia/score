#pragma once
#include <QString>

namespace Scenario
{
class ScenarioModel;
namespace Metrics
{
namespace Halstead
{
struct Factors
{
        double eta1{};
        double eta2{};
        double N1{};
        double N2{};
};
Factors ComputeFactors(const ScenarioModel& scenar);
inline double ProgramLength(const Factors& f)
{
    return f.eta1 * std::log2(f.eta1) + f.eta2 * std::log2(f.eta2);
}

inline double Difficulty(const Factors& f)
{
    return (f.eta1 / 2.) * (f.N2 / f.eta2);
}

inline double Volume(const Factors& f)
{
    return (f.N1 + f.N2) * std::log2(f.eta1 + f.eta2);

}
inline double Effort(const Factors& f)
{
    return Difficulty(f) * Volume(f);

}

inline double TimeRequired(const Factors& f)
{
    return Effort(f) / 18.;
}

inline double Bugs2(const Factors& f)
{
    return Volume(f) / 3000.;
}
}

namespace Cyclomatic
{
struct Factors
{
    Factors(int e, int n, int c):
        edgeCount{e},
        nodeCount{n},
        connectedComponents{c}
    {
    }
        int edgeCount{};
        int nodeCount{};
        int connectedComponents{};
};

Factors ComputeFactors(const ScenarioModel& scenar);
Factors ComputeFactors2(const ScenarioModel& scenar);

inline double Complexity(const Factors& f)
{
    return f.edgeCount - f.nodeCount + 2 * f.connectedComponents;
}
}

QString toScenarioLanguage(const Scenario::ScenarioModel& s);
}
}
