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

#ifndef STATE_HPP
#define STATE_HPP

#include <QState>

/*!
 *  This class permits to debug QState, by showing a message when entering and exiting the State.
 *
 *  @brief QState debug info
 *  @author Jaime Chao, Clément Bossut
 *  @date 2013/2014
 */

class StateDebug : public QState
{
  Q_OBJECT

public:
  explicit StateDebug( const QString& name, QState* parent = 0 );
  explicit StateDebug( const QString& name, const QString& prefix, QState* parent = 0 );

  QString name() const { return m_name; }
  QString prefix() const { return m_prefix; }

public slots:
  void setName( const QString& name ) { m_name = name; }
  void setPrefix( const QString& prefix ) { m_prefix = prefix; }

protected:
  virtual void onEntry( QEvent* e );
  virtual void onExit( QEvent* e );

protected:
  QString m_name;
  QString m_prefix;

};

#endif // STATE_HPP
