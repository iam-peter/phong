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
    const QCommandLineOption portOption(QStringLiteral("port"), QStringLiteral("The port to listen on"),
                                        QStringLiteral("port"), QStringLiteral("45460"));
    const QCommandLineOption dataOption(QStringLiteral("scores"),
                                        QStringLiteral("The file keeping the high scores, none by default"),
                                        QStringLiteral("file"));
    parser.addOption(portOption);
    parser.addOption(dataOption);
    parser.process(app);

    bool valid = false;
    const uint port = parser.value(portOption).toUInt(&valid);
    if (!valid || port > 65535) {
        std::fprintf(stderr, "Not a port: %s\n", qPrintable(parser.value(portOption)));
        return 1;
    }

    RelayServer server(parser.value(dataOption));
    if (!server.listen(quint16(port)))
        return 1;
    std::printf("Listening on port %u\n", unsigned(server.port()));
    std::fflush(stdout);
    return app.exec();
}
