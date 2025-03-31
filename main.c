#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <cairo/cairo.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct limebar {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwlr_layer_surface_v1 *layer_surface;
    uint32_t width;
    uint32_t height;
    struct wl_buffer *buffer;
    cairo_surface_t *cairo_surface;
    cairo_t *cairo;
    void *shm_data;
};

static void registry_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version);
static void registry_global_remove(void *data,
        struct wl_registry *registry, uint32_t name);

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static void create_buffer(struct limebar *bar) {
    int stride = bar->width * 4;
    int size = stride * bar->height;

    char name[] = "/tmp/limebar-XXXXXX";
    int fd = mkstemp(name);
    if (fd < 0) {
        fprintf(stderr, "Failed to create temp file: %s\n", strerror(errno));
        exit(1);
    }
    unlink(name);

    if (ftruncate(fd, size) == -1) {
        fprintf(stderr, "Failed to set file size: %s\n", strerror(errno));
        close(fd);
        exit(1);
    }

    bar->shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bar->shm_data == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap: %s\n", strerror(errno));
        close(fd);
        exit(1);
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(bar->shm, fd, size);
    bar->buffer = wl_shm_pool_create_buffer(pool, 0, bar->width, bar->height,
            stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    bar->cairo_surface = cairo_image_surface_create_for_data(bar->shm_data,
            CAIRO_FORMAT_ARGB32, bar->width, bar->height, stride);
    bar->cairo = cairo_create(bar->cairo_surface);
}

static void draw(struct limebar *bar) {
    // Clear the surface with a semi-transparent dark gray
    cairo_set_source_rgba(bar->cairo, 0.2, 0.2, 0.2, 0.9);
    cairo_paint(bar->cairo);

    // Draw text
    cairo_select_font_face(bar->cairo, "monospace",
            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(bar->cairo, 14);
    cairo_set_source_rgba(bar->cairo, 1.0, 1.0, 1.0, 1.0);  // White text
    cairo_move_to(bar->cairo, 10, 17);
    cairo_show_text(bar->cairo, "Limebar");

    // Flush Cairo operations
    cairo_surface_flush(bar->cairo_surface);

    // Attach and commit
    wl_surface_attach(bar->surface, bar->buffer, 0, 0);
    wl_surface_damage_buffer(bar->surface, 0, 0, bar->width, bar->height);
    wl_surface_commit(bar->surface);
}

static void registry_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    struct limebar *bar = data;
    (void)version;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        bar->compositor = wl_registry_bind(registry, name,
                &wl_compositor_interface, 4);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        bar->layer_shell = wl_registry_bind(registry, name,
                &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        bar->shm = wl_registry_bind(registry, name,
                &wl_shm_interface, 1);
    }
}

static void registry_global_remove(void *data,
        struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static void layer_surface_configure(void *data,
        struct zwlr_layer_surface_v1 *surface,
        uint32_t serial, uint32_t width, uint32_t height) {
    struct limebar *bar = data;

    // Only update dimensions if they changed
    if (width > 0) bar->width = width;
    if (height > 0) bar->height = height;

    zwlr_layer_surface_v1_ack_configure(surface, serial);

    // Clean up existing buffer if it exists
    if (bar->buffer) {
        wl_buffer_destroy(bar->buffer);
        cairo_destroy(bar->cairo);
        cairo_surface_destroy(bar->cairo_surface);
        munmap(bar->shm_data, bar->width * bar->height * 4);
        bar->buffer = NULL;
        bar->cairo = NULL;
        bar->cairo_surface = NULL;
        bar->shm_data = NULL;
    }

    create_buffer(bar);
    draw(bar);
}

static void layer_surface_closed(void *data,
        struct zwlr_layer_surface_v1 *surface) {
    (void)data;
    (void)surface;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    struct limebar bar = {0};
    bar.width = 1920;  // Default width
    bar.height = 24;   // Default height

    // Connect to Wayland display
    bar.display = wl_display_connect(NULL);
    if (!bar.display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    // Get registry
    bar.registry = wl_display_get_registry(bar.display);
    wl_registry_add_listener(bar.registry, &registry_listener, &bar);
    wl_display_roundtrip(bar.display);

    if (!bar.compositor || !bar.layer_shell || !bar.shm) {
        fprintf(stderr, "Missing required Wayland interfaces\n");
        return 1;
    }

    // Create surface
    bar.surface = wl_compositor_create_surface(bar.compositor);
    if (!bar.surface) {
        fprintf(stderr, "Failed to create surface\n");
        return 1;
    }

    // Create layer surface
    bar.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
            bar.layer_shell, bar.surface, NULL,
            ZWLR_LAYER_SHELL_V1_LAYER_TOP, "limebar");
    if (!bar.layer_surface) {
        fprintf(stderr, "Failed to create layer surface\n");
        return 1;
    }

    zwlr_layer_surface_v1_set_size(bar.layer_surface, bar.width, bar.height);
    zwlr_layer_surface_v1_set_anchor(bar.layer_surface,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_add_listener(bar.layer_surface,
            &layer_surface_listener, &bar);

    // Commit the surface and wait for the configure event
    wl_surface_commit(bar.surface);
    wl_display_roundtrip(bar.display);
    wl_display_roundtrip(bar.display); // Add a second roundtrip

    // Main event loop
    while (wl_display_dispatch(bar.display) != -1) {
        // Events handled in callbacks
    }

    // ... rest of cleanup code ...
    return 0;
}
