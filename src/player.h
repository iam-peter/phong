#ifndef PLAYER_H
#define PLAYER_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class Player : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Players are owned by a Match")
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int score READ score NOTIFY scoreChanged)
    Q_PROPERTY(bool computer READ isComputer WRITE setComputer NOTIFY computerChanged)

public:
    explicit Player(QObject* parent = nullptr);

    void setName(const QString& name);
    QString name() const;

    void setScore(int score);
    int score() const;

    void setComputer(bool computer);
    bool isComputer() const;

signals:
    void nameChanged(const QString&);
    void scoreChanged(int);
    void computerChanged(bool);

private:
    QString m_name;
    int m_score;
    bool m_computer;
};

#endif // PLAYER_H
