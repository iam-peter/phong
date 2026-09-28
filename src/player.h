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
    Q_PROPERTY(int sets READ sets NOTIFY setsChanged)
    // Paddle hits in the match
    Q_PROPERTY(int hits READ hits NOTIFY hitsChanged)
    Q_PROPERTY(bool computer READ isComputer WRITE setComputer NOTIFY computerChanged)

    // Modifier effects, set by Modifiers
    Q_PROPERTY(qreal paddleScale READ paddleScale WRITE setPaddleScale NOTIFY paddleScaleChanged)
    // Degrees per second the paddle rotates with, 0 is upright
    Q_PROPERTY(qreal spinSpeed READ spinSpeed WRITE setSpinSpeed NOTIFY spinSpeedChanged)
    Q_PROPERTY(bool shielded READ isShielded WRITE setShielded NOTIFY shieldedChanged)

public:
    explicit Player(QObject* parent = nullptr);

    void setName(const QString& name);
    QString name() const;

    void setScore(int score);
    int score() const;

    void setSets(int sets);
    int sets() const;

    void setHits(int hits);
    int hits() const;

    void setComputer(bool computer);
    bool isComputer() const;

    void setPaddleScale(qreal paddleScale);
    qreal paddleScale() const;

    void setSpinSpeed(qreal spinSpeed);
    qreal spinSpeed() const;

    void setShielded(bool shielded);
    bool isShielded() const;

signals:
    void nameChanged(const QString&);
    void scoreChanged(int);
    void setsChanged(int);
    void hitsChanged(int);
    void computerChanged(bool);
    void paddleScaleChanged(qreal);
    void spinSpeedChanged(qreal);
    void shieldedChanged(bool);

private:
    QString m_name;
    int m_score;
    int m_sets;
    int m_hits;
    bool m_computer;
    qreal m_paddleScale;
    qreal m_spinSpeed;
    bool m_shielded;
};

#endif // PLAYER_H
