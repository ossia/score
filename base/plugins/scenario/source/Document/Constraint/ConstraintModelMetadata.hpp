#pragma once
#include <QObject>
#include <QColor>

/**
 * @brief The ConstraintModelMetadata class
 *
 * Metadata for the constraint, in order to make ConstraintModel lighter
 *
 */
class ConstraintModelMetadata : public QObject
{
		Q_OBJECT
		Q_PROPERTY(QString name
				   READ name
				   WRITE setName
				   NOTIFY nameChanged)

		Q_PROPERTY(QString comment
				   READ comment
				   WRITE setComment
				   NOTIFY commentChanged)

		Q_PROPERTY(QColor color
				   READ color
				   WRITE setColor
				   NOTIFY colorChanged)

	public:
		QString name() const;
		QString comment() const;
		QColor color() const;

	signals:
		void nameChanged(QString arg);
		void commentChanged(QString arg);
		void colorChanged(QColor arg);

	public slots:
		void setName(QString arg);
		void setComment(QString arg);
		void setColor(QColor arg);

	private:
		QString m_name;
		QString m_comment;
		QColor m_color;
};

QDataStream& operator<<(QDataStream&, const ConstraintModelMetadata&);
QDataStream& operator>>(QDataStream&, ConstraintModelMetadata&);