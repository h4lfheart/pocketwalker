#include <iostream>
#include <QApplication>
#include <QStyleFactory>
#include <QSurfaceFormat>

#include "argparse/argparse.hpp"

#include "qt/settings/app_settings.h"
#include "qt/window/qt_window_system.h"

int main(int argc, char* argv[])
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    QSurfaceFormat::setDefaultFormat(fmt);

    argparse::ArgumentParser arguments("pocketwalker");

    arguments.add_argument("rom")
             .help("The path to the PokeWalker rom file.")
             .nargs(argparse::nargs_pattern::optional);

    arguments.add_argument("save")
             .help("The path to your PokeWalker save file.")
             .nargs(argparse::nargs_pattern::optional);

    arguments.add_argument("--server")
             .help("Runs in server mode.")
             .flag();

    arguments.add_argument("--no-menu")
             .help("Hides the main window menu bar.")
             .flag();

    arguments.add_argument("--ip")
             .help("IP address to connect to (client mode).");

    arguments.add_argument("--port")
             .help("TCP port for server or client.")
             .scan<'i', uint16_t>();

    try
    {
        arguments.parse_args(argc, argv);
    }
    catch (const std::exception& err)
    {
        std::cerr << err.what() << "\n" << arguments;
        return 1;
    }

    QApplication app(argc, argv);
    AppSettings::instance.load();

    ApplicationArguments args;
    args.rom_path = arguments.present<std::string>("rom");
    args.save_path = arguments.present<std::string>("save");
    args.server_mode = arguments.get<bool>("--server");
    args.no_menu = arguments.get<bool>("--no-menu");
    args.host = arguments.present<std::string>("--ip");
    args.port = arguments.present<uint16_t>("--port");

    QtWindowSystem window(args);
    window.show();

    const int result = app.exec();
    AppSettings::instance.save();
    return result;
}
