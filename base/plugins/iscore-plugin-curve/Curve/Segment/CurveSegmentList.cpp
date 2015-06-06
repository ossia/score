#include "CurveSegmentList.hpp"

CurveSegmentFactory*CurveSegmentList::get(const QString& name)
{
    return *std::find_if(factories.begin(), factories.end(),
                         [&] (auto&& p) { return p->name() == name; });
}

void CurveSegmentList::registerFactory(CurveSegmentFactory* fact)
{
    factories.push_back(fact);
}



// TODO check this on windows. see http://stackoverflow.com/a/7823764/1495627
CurveSegmentList &SingletonCurveSegmentList::instance()
{
    static CurveSegmentList m_instance;
    return m_instance;
}
