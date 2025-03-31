#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <cairo/cairo.h>
#include <getopt.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <pango/pango.h>
#include <pango/pangocairo.h>

struct bar_config {
    int width;
    int height;
    char *background_color;
    char **fonts;
    int num_fonts;
    int underline_thickness;
};

static void print_usage(const char *program_name) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -g, --geometry WxH+X+Y    Set bar geometry (e.g., 1920x24+0+0)\n"
        "  -B, --background COLOR    Set background color (e.g., #1a1a1a)\n"
        "  -f, --font FONT          Add font (can be used multiple times)\n"
        "  -u, --underline SIZE     Set underline thickness (default: 2)\n"
        "  -h, --help              Show this help message\n",
        program_name);
}

static void parse_geometry(const char *geometry, int *width, int *height, int *x, int *y) {
    sscanf(geometry, "%dx%d+%d+%d", width, height, x, y);
}

static void parse_color_str(const char *color, double *r, double *g, double *b, double *a) {
    if (color[0] == '#') {
        unsigned int rgb;
        if (strlen(color) == 7) { // #RRGGBB
            sscanf(color + 1, "%x", &rgb);
            *r = ((rgb >> 16) & 0xFF) / 255.0;
            *g = ((rgb >> 8) & 0xFF) / 255.0;
            *b = (rgb & 0xFF) / 255.0;
            *a = 1.0;
        } else if (strlen(color) == 9) { // #RRGGBBAA
            sscanf(color + 1, "%x", &rgb);
            *r = ((rgb >> 24) & 0xFF) / 255.0;
            *g = ((rgb >> 16) & 0xFF) / 255.0;
            *b = ((rgb >> 8) & 0xFF) / 255.0;
            *a = (rgb & 0xFF) / 255.0;
        }
    }
}

struct text_block {
    char *text;
    char *fg_color;
    char *bg_color;
    char *underline_color;
    int font_index;
    bool underline;
    enum {
        ALIGN_LEFT,
        ALIGN_CENTER,
        ALIGN_RIGHT
    } alignment;
    struct text_block *next;
};


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
    PangoContext *pango_context;
    PangoLayout *pango_layout;
    struct text_block *blocks;
    char **fonts;
    int num_fonts;

    struct {
            double r, g, b, a;
        } bg_color;
};

static struct text_block *parse_input(const char *input) {
    printf("Parsing input: %s\n", input);
    struct text_block *head = NULL;
    struct text_block *current = NULL;

    char *str = strdup(input);
    char *saveptr1;
    char *token = strtok_r(str, "[]", &saveptr1);

    while (token) {
        // Skip leading whitespace
        while (*token == ' ') token++;

        if (strchr(token, ':')) {
            struct text_block *block = calloc(1, sizeof(struct text_block));

            char *attrs_text = strdup(token);
            char *saveptr2;
            char *attrs = strtok_r(attrs_text, ":", &saveptr2);
            char *text = strtok_r(NULL, ":", &saveptr2);

            printf("Parsing block - attrs: %s, text: %s\n", attrs, text);

            char *saveptr3;
            char *attr = strtok_r(attrs, ",", &saveptr3);
            while (attr) {
                printf("Processing attribute: %s\n", attr);
                switch (attr[0]) {
                    case 'F':
                        block->fg_color = strdup(attr + 2);
                        printf("Set fg_color: %s\n", block->fg_color);
                        break;
                    case 'B':
                        block->bg_color = strdup(attr + 2);
                        break;
                    case 'U':
                        block->underline_color = strdup(attr + 2);
                        break;
                    case 'T':
                        block->font_index = atoi(attr + 2) - 1;
                        break;
                    case 'u':
                        block->underline = true;
                        printf("Set underline: true\n");
                        break;
                }
                attr = strtok_r(NULL, ",", &saveptr3);
            }

            block->text = text ? strdup(text) : strdup("");

            if (!head) {
                head = block;
                current = block;
            } else {
                current->next = block;
                current = block;
            }
            free(attrs_text);
        }
        token = strtok_r(NULL, "[]", &saveptr1);
    }

    // Print all blocks for debugging
    struct text_block *block = head;
    while (block) {
        printf("Block - text: %s, fg_color: %s, underline: %d\n",
               block->text, block->fg_color ? block->fg_color : "none",
               block->underline);
        block = block->next;
    }

    free(str);
    return head;
}

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

static void parse_color(const char *color_str, double *r, double *g, double *b) {
    printf("Parsing color: %s\n", color_str);  // Debug print
    if (color_str[0] == '#') {
        unsigned int color;
        sscanf(color_str + 1, "%x", &color);
        *r = ((color >> 16) & 0xFF) / 255.0;
        *g = ((color >> 8) & 0xFF) / 255.0;
        *b = (color & 0xFF) / 255.0;
        printf("Parsed color values: r=%f, g=%f, b=%f\n", *r, *g, *b);  // Debug print
    } else {
        *r = 1.0;
        *g = 1.0;
        *b = 1.0;
    }
}

static void draw(struct limebar *bar) {
    printf("Drawing started\n");

        cairo_set_source_rgba(bar->cairo,
            bar->bg_color.r,
            bar->bg_color.g,
            bar->bg_color.b,
            bar->bg_color.a);
        cairo_paint(bar->cairo);

    if (!bar->pango_context) {
        bar->pango_context = pango_cairo_create_context(bar->cairo);
        bar->pango_layout = pango_layout_new(bar->pango_context);
    }

    int x = 10;
    struct text_block *block = bar->blocks;

    while (block) {
        printf("Drawing block - text: %s\n", block->text);  // Debug print

        PangoFontDescription *font_desc = pango_font_description_from_string(
            bar->fonts[block->font_index]);
        pango_layout_set_font_description(bar->pango_layout, font_desc);

        if (block->fg_color) {
            double r, g, b;
            parse_color(block->fg_color, &r, &g, &b);
            cairo_set_source_rgb(bar->cairo, r, g, b);
            printf("Setting color: r=%f, g=%f, b=%f\n", r, g, b);  // Debug print
        } else {
            cairo_set_source_rgb(bar->cairo, 1.0, 1.0, 1.0);
        }

        pango_layout_set_text(bar->pango_layout, block->text, -1);

        int width, height;
        pango_layout_get_pixel_size(bar->pango_layout, &width, &height);
        printf("Text dimensions: width=%d, height=%d\n", width, height);  // Debug print

        cairo_move_to(bar->cairo, x, (bar->height - height) / 2);
        pango_cairo_show_layout(bar->cairo, bar->pango_layout);

        if (block->underline) {
            printf("Drawing underline\n");  // Debug print
            double r, g, b;
            if (block->underline_color) {
                parse_color(block->underline_color, &r, &g, &b);
            } else if (block->fg_color) {
                parse_color(block->fg_color, &r, &g, &b);
            } else {
                r = g = b = 1.0;
            }

            cairo_set_source_rgb(bar->cairo, r, g, b);
            cairo_set_line_width(bar->cairo, 2.0);
            cairo_move_to(bar->cairo, x, bar->height - 2);
            cairo_line_to(bar->cairo, x + width, bar->height - 2);
            cairo_stroke(bar->cairo);
        }

        x += width + 10;
        pango_font_description_free(font_desc);
        block = block->next;
    }

    printf("Drawing completed\n");  // Debug print

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
    struct bar_config config = {
            .width = 1920,
            .height = 24,
            .background_color = "#1a1a1a",
            .fonts = NULL,
            .num_fonts = 0,
            .underline_thickness = 2
        };

        // Define command line options
        static struct option long_options[] = {
            {"geometry", required_argument, 0, 'g'},
            {"background", required_argument, 0, 'B'},
            {"font", required_argument, 0, 'f'},
            {"underline", required_argument, 0, 'u'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        // Parse command line options
        int opt;
        while ((opt = getopt_long(argc, argv, "g:B:f:u:h", long_options, NULL)) != -1) {
            switch (opt) {
                case 'g': {
                    int x, y;
                    parse_geometry(optarg, &config.width, &config.height, &x, &y);
                    break;
                }
                case 'B':
                    config.background_color = optarg;
                    break;
                case 'f': {
                    config.num_fonts++;
                    config.fonts = realloc(config.fonts, sizeof(char*) * config.num_fonts);
                    config.fonts[config.num_fonts - 1] = strdup(optarg);
                    break;
                }
                case 'u':
                    config.underline_thickness = atoi(optarg);
                    break;
                case 'h':
                    print_usage(argv[0]);
                    return 0;
                default:
                    print_usage(argv[0]);
                    return 1;
            }
        }

        // Set default fonts if none specified
        if (config.num_fonts == 0) {
            config.num_fonts = 2;
            config.fonts = malloc(sizeof(char*) * config.num_fonts);
            config.fonts[0] = strdup("Monospace 12");
            config.fonts[1] = strdup("Monospace Bold 12");
        }

        struct limebar bar = {0};
            bar.width = config.width;
            bar.height = config.height;
            bar.num_fonts = config.num_fonts;
            bar.fonts = config.fonts;

            // Set background color
            parse_color_str(config.background_color,
                &bar.bg_color.r,
                &bar.bg_color.g,
                &bar.bg_color.b,
                &bar.bg_color.a);

            // Remove duplicate font initialization
            // bar.num_fonts = 2;
            // bar.fonts = malloc(sizeof(char*) * bar.num_fonts);
            // bar.fonts[0] = "Monospace 12";
            // bar.fonts[1] = "Monospace Bold 12";

            const char *test_input = "[F=#ffffff,T=1:Hello] [F=#ff0000,u:World]";
            bar.blocks = parse_input(test_input);

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

    create_buffer(&bar);
        draw(&bar);

        wl_surface_commit(bar.surface);
        wl_display_roundtrip(bar.display);
        wl_display_roundtrip(bar.display);

    // Main event loop
    while (wl_display_dispatch(bar.display) != -1) {
        // Events handled in callbacks
    }

    // Cleanup
        struct text_block *block = bar.blocks;
        while (block) {
            struct text_block *next = block->next;
            free(block->text);
            free(block->fg_color);
            free(block->bg_color);
            free(block->underline_color);
            free(block);
            block = next;
        }
        for (int i = 0; i < config.num_fonts; i++) {
                free(config.fonts[i]);
            }
            free(config.fonts);

        if (bar.cairo)
            cairo_destroy(bar.cairo);
        if (bar.cairo_surface)
            cairo_surface_destroy(bar.cairo_surface);
        if (bar.buffer)
            wl_buffer_destroy(bar.buffer);
        if (bar.shm_data)
            munmap(bar.shm_data, bar.width * bar.height * 4);
        if (bar.layer_surface)
            zwlr_layer_surface_v1_destroy(bar.layer_surface);
        if (bar.surface)
            wl_surface_destroy(bar.surface);
        if (bar.layer_shell)
            zwlr_layer_shell_v1_destroy(bar.layer_shell);
        if (bar.shm)
            wl_shm_destroy(bar.shm);
        if (bar.compositor)
            wl_compositor_destroy(bar.compositor);
        if (bar.registry)
            wl_registry_destroy(bar.registry);
        if (bar.display)
            wl_display_disconnect(bar.display);

        return 0;
}
