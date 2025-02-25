#pragma once
#include <wayland-client.h>
#include <string>
#include <cstring>

struct LimeBar {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
};

void registry_global(void *data, struct wl_registry *registry,
                    uint32_t name, const char *interface, uint32_t version);
void registry_global_remove(void *data, struct wl_registry *registry,
                          uint32_t name);
