// ibus-engine-july-bangla: the IBus engine process for July Bangla Keyboard.
//
//   --ibus        started by ibus-daemon (normal use, from the component file)
//   (no option)   development: registers the component itself with a running IBus
//   --self-test   checks the engine without IBus (used by ctest)

#include <cstdio>
#include <cstring>

#include "Engine.h"

int main(int argc, char** argv) {
    bool startedByIBus = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--self-test") == 0) return julySelfTest();
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("ibus-engine-july-bangla %s\n", JULY_VERSION);
            return 0;
        }
        if (std::strcmp(argv[i], "--ibus") == 0) startedByIBus = true;
    }

    ibus_init();
    IBusBus* bus = ibus_bus_new();
    if (!ibus_bus_is_connected(bus)) {
        std::fprintf(stderr, "Cannot connect to IBus. Is ibus-daemon running?\n");
        return 1;
    }
    g_signal_connect(bus, "disconnected", G_CALLBACK(+[](IBusBus*, gpointer) { ibus_quit(); }), nullptr);

    IBusFactory* factory = ibus_factory_new(ibus_bus_get_connection(bus));
    ibus_factory_add_engine(factory, "july-bangla", july_ibus_engine_get_type());

    if (startedByIBus) {
        ibus_bus_request_name(bus, "org.freedesktop.IBus.JulyBangla", 0);
    } else {
        IBusComponent* component = ibus_component_new(
            "org.freedesktop.IBus.JulyBangla", "July Bangla Keyboard", JULY_VERSION, "MIT",
            "July Bangla Keyboard project", "https://julykeyboard.vercel.app/", "", "ibus-july-bangla");
        ibus_component_add_engine(component,
                                  ibus_engine_desc_new("july-bangla", "July Bangla Keyboard",
                                                       "Bangla with the Bijoy-compatible layout", "bn", "MIT",
                                                       "July Bangla Keyboard project", "", "us"));
        ibus_bus_register_component(bus, component);
    }

    ibus_main();
    return 0;
}
