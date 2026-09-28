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

    // Modifier effects, set by Modifiers
    Q_PROPERTY(qreal paddleScale READ paddleScale WRITE setPaddleScale NOTIFY paddleScaleChanged)
    Q_PROPERTY(bool spinning READ isSpinning WRITE setSpinning NOTIFY spinningChanged)
    Q_PROPERTY(bool shielded READ isShielded WRITE setShielded NOTIFY shieldedChanged)

public:
    explicit Player(QObject* parent = nullptr);

    void setName(const QString& name);
    QString name() const;

    void setScore(int score);
    int score() const;

    void setComputer(bool computer);
    bool isComputer() const;

    void setPaddleScale(qreal paddleScale);
    qreal paddleScale() const;

    void setSpinning(bool spinning);
    bool isSpinning() const;

    void setShielded(bool shielded);
    bool isShielded() const;

signals:
    void nameChanged(const QString&);
    void scoreChanged(int);
    void computerChanged(bool);
    void paddleScaleChanged(qreal);
    void spinningChanged(bool);
    void shieldedChanged(bool);

private:
    QString m_name;
    int m_score;
    bool m_computer;
    qreal m_paddleScale;
    bool m_spinning;
    bool m_shielded;
};

#endif // PLAYER_H
