#include "relayserver.h"

#include <QCommandLineParser>
#include <QCoreApplication>

#include <cstdio>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("phong-server"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Rooms for P(H)ONG over the internet and its shared high scores"));
    parser.addHelpOption();
    // Hosting services like Render say in PORT where to listen
    const QCommandLineOption portOption(QStringLiteral("port"),
                                        QStringLiteral("The port to listen on, $PORT or 45460 by default"),
                                        QStringLiteral("port"),
                                        qEnvironmentVariable("PORT", QStringLiteral("45460")));
    const QCommandLineOption dataOption(QStringLiteral("scores"),
                                        QStringLiteral("The file keeping the high scores, none by default"),
                                        QStringLiteral("file"));
    const QCommandLineOption lagOption(QStringLiteral("lag"),
                                       QStringLiteral("Holds the messages of the games back, for trying"),
                                       QStringLiteral("ms"), QStringLiteral("0"));
    parser.addOption(portOption);
    parser.addOption(dataOption);
    parser.addOption(lagOption);
    parser.process(app);

    bool valid = false;
    const uint port = parser.value(portOption).toUInt(&valid);
    if (!valid || port > 65535) {
        std::fprintf(stderr, "Not a port: %s\n", qPrintable(parser.value(portOption)));
        return 1;
    }

    RelayServer server(parser.value(dataOption));
    server.setLag(parser.value(lagOption).toInt());
    if (!server.listen(quint16(port)))
        return 1;
    std::printf("Listening on port %u\n", unsigned(server.port()));
    std::fflush(stdout);
    return app.exec();
}
