#include "MovePointCommandObject.hpp"
#include "UpdateCurve.hpp"
#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include <iscore/document/DocumentInterface.hpp>

MovePointCommandObject::MovePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack):
    CurveCommandObjectBase(nullptr),
    m_dispatcher{stack}
{

}

void MovePointCommandObject::on_press()
{
    // Create the command. For now nothing changes.
    m_dispatcher.submitCommand<UpdateCurve>(iscore::IDocument::path(m_presenter->model()),
                                            m_originalSegments);

}

void MovePointCommandObject::move()
{
    // Two cases :
    // Point is on single curve
    // Point is on two curves
    // Where to take the current clicked point ?
    // We have to know it so that we can get the both borders
    m_dispatcher.submitCommand<UpdateCurve>(iscore::IDocument::path(m_presenter->model()),
                                            m_originalSegments);
}

void MovePointCommandObject::release()
{
    m_dispatcher.commit();
}

void MovePointCommandObject::cancel()
{
    m_dispatcher.rollback();
}
