#include "CurveSegmentList.hpp"

CurveSegmentFactory* CurveSegmentList::get(const QString& name) const
{
    // TODO here we should really use a map.
    auto it = std::find_if(factories.begin(), factories.end(),
                         [&] (auto&& p) { return p->name() == name; });

    ISCORE_ASSERT(it != factories.end());

    return *it;
}

void CurveSegmentList::registerFactory(CurveSegmentFactory* fact)
{
    factories.push_back(fact);
}

QStringList CurveSegmentList::nameList() const
{
    QStringList l;
    for(auto& element : factories)
    {
        l.append(element->name());
    }

    return l;
}



// TODO check this on windows. see http://stackoverflow.com/a/7823764/1495627
CurveSegmentList &SingletonCurveSegmentList::instance()
{
    static CurveSegmentList m_instance;
    return m_instance;
}
