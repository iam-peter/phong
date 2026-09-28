#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include "computerplayer.h"

#include <QObject>
#include <QRandomGenerator>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// A knockout bracket of eight: the player and seven computer players with
// their own personalities. The player's matches are played, the others
// are decided by the strength of the two.
class Tournament : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Round of the player's next match, 0 is the quarterfinal
    Q_PROPERTY(int round READ round NOTIFY changed)
    // The player is still in and has a match to play
    Q_PROPERTY(bool running READ isRunning NOTIFY changed)
    Q_PROPERTY(bool champion READ isChampion NOTIFY changed)
    // The player's next opponent: name, personality, difficulty
    Q_PROPERTY(QVariantMap opponent READ opponent NOTIFY changed)
    // For each round and a last one for the champion, the entrants in
    // bracket order: name, player, and result, see Result
    Q_PROPERTY(QVariantList rounds READ rounds NOTIFY changed)

public:
    static constexpr int size = 8;
    static constexpr int roundCount = 3;

    enum Result {
        Open = 0,
        Won,
        Lost
    };
    Q_ENUM(Result)

    struct Entrant {
        QString name;
        ComputerPlayer::Personality personality;
        ComputerPlayer::Difficulty difficulty;
        bool player;
    };

    explicit Tournament(QObject* parent = nullptr);

    static QList<Entrant> computerEntrants();

    // A new draw, the player's first match is next
    Q_INVOKABLE void start();
    // The player's match is over, the other matches of the round are
    // decided with it
    Q_INVOKABLE void recordResult(bool won);

    int round() const;
    bool isRunning() const;
    bool isChampion() const;
    QVariantMap opponent() const;
    QVariantList rounds() const;

    // Entrants of a round in bracket order, index into the draw
    QList<int> entrants(int round) const;

    void setSeed(quint32 seed);

signals:
    void changed();

private:
    // Winner of a match between the two computer players
    int simulate(int a, int b);
    void decideRound(int round, int playerWinner);
    int playerSlot(int round) const;

    QList<Entrant> m_draw;
    // m_winners[round][match] is the winning entrant, -1 while open
    QList<QList<int>> m_winners;
    int m_round;
    bool m_eliminated;
    QRandomGenerator m_random;
};

#endif // TOURNAMENT_H
