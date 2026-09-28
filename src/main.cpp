#include "arenas.h"
#include "modifiers.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char** argv)
{
    QGuiApplication::setOrganizationName(QStringLiteral("phong"));
    QGuiApplication::setApplicationName(QStringLiteral("Phong"));

    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption modifiersOption(
        QStringLiteral("modifiers"),
        QStringLiteral("Load the modifier definitions from <file> instead of the built-in ones."),
        QStringLiteral("file"));
    const QCommandLineOption arenasOption(
        QStringLiteral("arenas"),
        QStringLiteral("Load the arenas from <file> instead of the built-in ones."),
        QStringLiteral("file"));
    parser.addOptions({ modifiersOption, arenasOption });
    parser.process(app);

    if (parser.isSet(modifiersOption))
        Modifiers::setDefaultSource(parser.value(modifiersOption));
    if (parser.isSet(arenasOption))
        Arenas::setDefaultSource(parser.value(arenasOption));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(EXIT_FAILURE); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Phong", "Main");

    return app.exec();
}
