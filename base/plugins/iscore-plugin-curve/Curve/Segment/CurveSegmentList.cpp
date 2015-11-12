#include "CurveSegmentList.hpp"

// TODO check this on windows. see http://stackoverflow.com/a/7823764/1495627
CurveSegmentList &SingletonCurveSegmentList::instance()
{
    static CurveSegmentList m_instance;
    return m_instance;
}
