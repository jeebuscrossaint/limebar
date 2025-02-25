#include "main.h"
#include <iostream>
#include <cstdlib>

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

void registry_global(void *data, struct wl_registry *registry,
                    uint32_t name, const char *interface, uint32_t version) {
    LimeBar *bar = static_cast<LimeBar*>(data);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        bar->compositor = static_cast<struct wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 1));
    }
}

void registry_global_remove(void *data, struct wl_registry *registry,
                          uint32_t name) {
    // Handle removal of global objects
}

int main() {
    LimeBar bar = {};

    // Connect to Wayland display
    bar.display = wl_display_connect(nullptr);
    if (!bar.display) {
        std::cerr << "Failed to connect to Wayland display\n";
        return 1;
    }

    // Get registry
    bar.registry = wl_display_get_registry(bar.display);
    wl_registry_add_listener(bar.registry, &registry_listener, &bar);

    // Initial roundtrip to receive registry events
    wl_display_roundtrip(bar.display);

    if (!bar.compositor) {
        std::cerr << "No compositor found\n";
        return 1;
    }

    // Create surface
    bar.surface = wl_compositor_create_surface(bar.compositor);
    if (!bar.surface) {
        std::cerr << "Failed to create surface\n";
        return 1;
    }

    std::cout << "Created Wayland surface successfully!\n";

    // Main event loop
    while (wl_display_dispatch(bar.display) != -1) {
        // Handle events
    }

    // Cleanup
    if (bar.surface)
        wl_surface_destroy(bar.surface);
    if (bar.compositor)
        wl_compositor_destroy(bar.compositor);
    if (bar.registry)
        wl_registry_destroy(bar.registry);
    if (bar.display)
        wl_display_disconnect(bar.display);

    return 0;
}
