#include "CurveSegmentList.hpp"

CurveSegmentFactory*CurveSegmentList::get(const QString& name)
{
    return *std::find_if(factories.begin(), factories.end(),
                         [&] (auto&& p) { return p->name() == name; });
}

void CurveSegmentList::registration(CurveSegmentFactory* fact)
{
    factories.push_back(fact);
}

CurveSegmentList*CurveSegmentList::instance()
{
    static auto ptr = new ::CurveSegmentList;
    return ptr;
}
