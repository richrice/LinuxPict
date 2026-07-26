#include "linuxpict/app.hpp"
#include "linuxpict/capture.hpp"

#include <gtkmm.h>

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (std::find(arguments.begin(), arguments.end(), "--help") != arguments.end()) {
        std::cout << "Usage: linuxpict [--capture]\n"
                     "Capture and annotate screenshots for an AI agent.\n";
        return 0;
    }
    const bool capture =
        std::find(arguments.begin(), arguments.end(), "--capture") != arguments.end();
    auto application = Gtk::Application::create(
        "com.github.richrice.LinuxPict",
        Gio::APPLICATION_NON_UNIQUE
    );
    try {
        application->register_application();
        if (capture) {
            linuxpict::AnnotationWindow window(application, linuxpict::capture_with_portal());
            return application->run(window);
        }
        linuxpict::LauncherWindow window(application);
        return application->run(window);
    } catch (const linuxpict::CaptureCancelled&) {
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "LinuxPict: " << error.what() << '\n';
        return 1;
    }
}
