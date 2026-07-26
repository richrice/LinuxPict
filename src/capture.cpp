#include "linuxpict/capture.hpp"

#include <gio/gio.h>
#include <glib.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>

namespace linuxpict {
namespace {

struct PortalResult {
    GMainLoop* loop{};
    unsigned response{2};
    std::string uri;
};

void on_response(
    GDBusConnection*,
    const gchar*,
    const gchar*,
    const gchar*,
    const gchar*,
    GVariant* parameters,
    gpointer user_data
) {
    auto& result = *static_cast<PortalResult*>(user_data);
    GVariant* values = nullptr;
    g_variant_get(parameters, "(u@a{sv})", &result.response, &values);
    const char* uri = nullptr;
    if (g_variant_lookup(values, "uri", "&s", &uri) && uri) {
        result.uri = uri;
    }
    g_variant_unref(values);
    g_main_loop_quit(result.loop);
}

std::string glib_error_message(GError* error, const std::string& fallback) {
    if (!error) {
        return fallback;
    }
    std::string message = error->message;
    g_error_free(error);
    return message;
}

}  // namespace

std::filesystem::path temporary_png_path() {
    std::string pattern = (std::filesystem::temp_directory_path() / "LinuxPict-XXXXXX.png").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    const auto suffix_length = 4;
    const int descriptor = mkstemps(writable.data(), suffix_length);
    if (descriptor < 0) {
        throw std::runtime_error("Could not create a temporary PNG");
    }
    close(descriptor);
    return writable.data();
}

std::filesystem::path capture_with_portal() {
    GError* error = nullptr;
    auto* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!connection) {
        throw std::runtime_error(glib_error_message(error, "Could not connect to the desktop bus"));
    }

    const std::string token = "linuxpict" + std::to_string(getpid());
    std::string sender = g_dbus_connection_get_unique_name(connection);
    sender.erase(0, sender.find_first_not_of(':'));
    for (auto& character : sender) {
        if (character == '.') {
            character = '_';
        }
    }
    const std::string request_path =
        "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;

    PortalResult result;
    result.loop = g_main_loop_new(nullptr, FALSE);
    const auto subscription = g_dbus_connection_signal_subscribe(
        connection,
        "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Request",
        "Response",
        request_path.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_response,
        &result,
        nullptr
    );

    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
    g_variant_builder_add(&options, "{sv}", "interactive", g_variant_new_boolean(FALSE));
    auto* reply = g_dbus_connection_call_sync(
        connection,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot",
        "Screenshot",
        g_variant_new("(sa{sv})", "", &options),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    if (!reply) {
        g_dbus_connection_signal_unsubscribe(connection, subscription);
        g_main_loop_unref(result.loop);
        g_object_unref(connection);
        throw std::runtime_error(glib_error_message(error, "Screenshot portal failed"));
    }
    g_variant_unref(reply);
    g_main_loop_run(result.loop);
    g_dbus_connection_signal_unsubscribe(connection, subscription);
    g_main_loop_unref(result.loop);
    g_object_unref(connection);

    if (result.response != 0) {
        throw CaptureCancelled{};
    }
    if (result.uri.empty()) {
        throw std::runtime_error("The desktop portal returned no screenshot");
    }

    error = nullptr;
    gchar* source_name = g_filename_from_uri(result.uri.c_str(), nullptr, &error);
    if (!source_name) {
        throw std::runtime_error(glib_error_message(error, "Unsupported screenshot URI"));
    }
    const std::filesystem::path source(source_name);
    g_free(source_name);
    const auto destination = temporary_png_path();
    std::error_code copy_error;
    std::filesystem::copy_file(
        source,
        destination,
        std::filesystem::copy_options::overwrite_existing,
        copy_error
    );
    if (copy_error) {
        std::filesystem::remove(destination);
        throw std::runtime_error("Could not copy portal screenshot: " + copy_error.message());
    }
    std::filesystem::permissions(destination, std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
    return destination;
}

}  // namespace linuxpict
