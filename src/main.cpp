#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char** argv)
{
    QGuiApplication::setOrganizationName(QStringLiteral("phong"));
    QGuiApplication::setApplicationName(QStringLiteral("Phong"));

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(EXIT_FAILURE); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Phong", "Main");

    return app.exec();
}
