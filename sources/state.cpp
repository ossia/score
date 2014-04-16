/*
Copyright: LaBRI / SCRIME

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software.  You can  use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and,  more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/

#include "state.hpp" 
#include <QDebug>

State::State( const QString& name, QState* parent )
    : QState( parent ),
      m_name( name ),
      m_prefix()
{
}

State::State( const QString& name, const QString& prefix, QState* parent )
    : QState( parent ),
      m_name( name ),
      m_prefix( prefix )
{
}

void State::onEntry( QEvent* e )
{
    Q_UNUSED( e );

    // Print out the state we are entering and it's parents
    QString state = m_name;
    State* parent = dynamic_cast<State*>( parentState() );
    while ( parent != 0 )
    {
        state = parent->name() + "->" + state;
        parent = dynamic_cast<State*>( parent->parentState() );
    }
    qDebug() << m_prefix << "Entering state:" << state;
}

void State::onExit( QEvent* e )
{
    Q_UNUSED( e );

    // Print out the state we are exiting and it's parents
    QString state = m_name;
    State* parent = dynamic_cast<State*>( parentState() );
    while ( parent != 0 )
    {
        state = parent->name() + "->" + state;
        parent = dynamic_cast<State*>( parent->parentState() );
    }
    qDebug() << m_prefix << "Exiting state:" << state;
}
