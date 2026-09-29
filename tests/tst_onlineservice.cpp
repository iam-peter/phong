#include "onlineservice.h"

#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class tst_OnlineService : public QObject
{
    Q_OBJECT

private:
    static void write(const QString& path, const QByteArray& content)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
    }

private slots:
    void initTestCase()
    {
        // The last published server is kept in the settings
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("phong-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_onlineservice"));
        QSettings().clear();
    }

    void publishedCustomAndOff()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("online.json"));
        write(path, "{ \"server\": \"wss://phong.example.com\" }");

        OnlineService online;
        QVERIFY(!online.isAvailable());
        online.setDirectory(QUrl::fromLocalFile(path));
        online.refresh();
        QTRY_COMPARE(online.publishedServer(), QStringLiteral("wss://phong.example.com"));
        QVERIFY(!online.isBusy());
        QCOMPARE(online.server(), QStringLiteral("wss://phong.example.com"));
        QVERIFY(online.isAvailable());

        // An own server goes first, switched off there is none
        online.setCustomServer(QStringLiteral(" 192.168.1.5:45460 "));
        QCOMPARE(online.server(), QStringLiteral("192.168.1.5:45460"));
        online.setEnabled(false);
        QCOMPARE(online.server(), QString());
        QVERIFY(!online.isAvailable());
        online.setEnabled(true);
        online.setCustomServer(QString());

        // Kept for a start without the directory
        QCOMPARE(OnlineService().publishedServer(), QStringLiteral("wss://phong.example.com"));

        // An empty server switches online play off for everybody
        write(path, "{ \"server\": \"\" }");
        QSignalSpy changed(&online, &OnlineService::serverChanged);
        online.refresh();
        QTRY_COMPARE(changed.count(), 1);
        QVERIFY(!online.isAvailable());
    }

    void brokenDirectoryKeepsTheLastAnswer()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("online.json"));
        write(path, "{ \"server\": \"wss://a.example.com\" }");
        OnlineService online;
        online.setDirectory(QUrl::fromLocalFile(path));
        online.refresh();
        QTRY_COMPARE(online.publishedServer(), QStringLiteral("wss://a.example.com"));

        // Gone, or something else than the directory
        online.setDirectory(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.json"))));
        online.refresh();
        QTRY_VERIFY(!online.isBusy());
        write(path, "<html>not found</html>");
        online.setDirectory(QUrl::fromLocalFile(path));
        online.refresh();
        QTRY_VERIFY(!online.isBusy());
        QCOMPARE(online.publishedServer(), QStringLiteral("wss://a.example.com"));
    }
};

QTEST_GUILESS_MAIN(tst_OnlineService)
#include "tst_onlineservice.moc"
