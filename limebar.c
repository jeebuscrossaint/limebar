#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <cairo/cairo.h>
#include <poll.h>
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
    bool raw_mode;        // Add this for raw text mode
        char *text_color;     // Add this for default text

    // New options
    int padding;           // Text padding
    enum {
        ALIGN_DEFAULT_LEFT,
        ALIGN_DEFAULT_CENTER,
        ALIGN_DEFAULT_RIGHT
    } default_alignment;
    enum {
        POSITION_TOP,
        POSITION_BOTTOM
    } position;
    int margin_top;
    int margin_bottom;
    int margin_left;
    int margin_right;
    char *separator;       // Block separator
    double opacity;        // Background opacity
};

static void print_usage(const char *program_name) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -r, --raw              Enable raw text mode (no block parsing)\n"
        "  -F, --text-color COLOR Set default text color for raw mode\n"
        "  -g, --geometry WxH+X+Y    Set bar geometry (e.g., 1920x24+0+0)\n"
        "  -B, --background COLOR    Set background color (e.g., #1a1a1a)\n"
        "  -f, --font FONT          Add font (can be used multiple times)\n"
        "  -u, --underline SIZE     Set underline thickness (default: 2)\n"
        "  -p, --padding SIZE       Set text padding (default: 10)\n"
        "  -a, --alignment POS      Set default alignment (left|center|right)\n"
        "  -t, --position POS       Set bar position (top|bottom)\n"
        "  -m, --margin MARGINS     Set margins (top,right,bottom,left)\n"
        "  -s, --separator STRING   Set block separator\n"
        "  -o, --opacity FLOAT      Set background opacity (0.0-1.0)\n"
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

      struct bar_config *config;  // Add this

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

    // Clear with background color and opacity
    cairo_set_source_rgba(bar->cairo,
        bar->bg_color.r,
        bar->bg_color.g,
        bar->bg_color.b,
        bar->bg_color.a * bar->config->opacity);
    cairo_paint(bar->cairo);

    if (!bar->pango_context) {
        bar->pango_context = pango_cairo_create_context(bar->cairo);
        bar->pango_layout = pango_layout_new(bar->pango_context);
    }

    // First pass: calculate total width and widths for each block
    int total_width = 0;
    int num_blocks = 0;
    struct {
        int width;
        int height;
        struct text_block *block;
    } *block_dims;

    // Count blocks and allocate array
    for (struct text_block *block = bar->blocks; block; block = block->next) num_blocks++;
    block_dims = calloc(num_blocks, sizeof(*block_dims));

    // Calculate dimensions
    int idx = 0;
    for (struct text_block *block = bar->blocks; block; block = block->next) {
        PangoFontDescription *font_desc = pango_font_description_from_string(
            bar->fonts[block->font_index]);
        pango_layout_set_font_description(bar->pango_layout, font_desc);
        pango_layout_set_text(bar->pango_layout, block->text, -1);

        pango_layout_get_pixel_size(bar->pango_layout,
            &block_dims[idx].width,
            &block_dims[idx].height);
        block_dims[idx].block = block;

        total_width += block_dims[idx].width;
        if (idx > 0 && bar->config->separator) {
            total_width += strlen(bar->config->separator) * 8; // Approximate separator width
        }
        total_width += bar->config->padding * 2;

        pango_font_description_free(font_desc);
        idx++;
    }

    // Calculate starting x position based on alignment
    int start_x = bar->config->margin_left;
    int available_width = bar->width - bar->config->margin_left - bar->config->margin_right;

    if (bar->config->default_alignment == ALIGN_DEFAULT_CENTER) {
        start_x = (available_width - total_width) / 2 + bar->config->margin_left;
    } else if (bar->config->default_alignment == ALIGN_DEFAULT_RIGHT) {
        start_x = available_width - total_width + bar->config->margin_left;
    }

    // Second pass: actual drawing
    int x = start_x;
    for (int i = 0; i < num_blocks; i++) {
        struct text_block *block = block_dims[i].block;
        int width = block_dims[i].width;
        int height = block_dims[i].height;

        // Draw background if specified
        if (block->bg_color) {
            double r, g, b;
            parse_color(block->bg_color, &r, &g, &b);
            cairo_set_source_rgb(bar->cairo, r, g, b);
            cairo_rectangle(bar->cairo,
                x - bar->config->padding,
                bar->config->margin_top,
                width + bar->config->padding * 2,
                bar->height - bar->config->margin_top - bar->config->margin_bottom);
            cairo_fill(bar->cairo);
        }

        // Set font and draw text
        PangoFontDescription *font_desc = pango_font_description_from_string(
            bar->fonts[block->font_index]);
        pango_layout_set_font_description(bar->pango_layout, font_desc);
        pango_layout_set_text(bar->pango_layout, block->text, -1);

        // Set text color
        if (block->fg_color) {
            double r, g, b;
            parse_color(block->fg_color, &r, &g, &b);
            cairo_set_source_rgb(bar->cairo, r, g, b);
        } else {
            cairo_set_source_rgb(bar->cairo, 1.0, 1.0, 1.0);
        }

        // Draw text
        int y = (bar->height - height) / 2;
        if (bar->config->position == POSITION_TOP) {
            y += bar->config->margin_top;
        } else {
            y += bar->config->margin_bottom;
        }

        cairo_move_to(bar->cairo, x, y);
        pango_cairo_show_layout(bar->cairo, bar->pango_layout);

        // Draw underline
        if (block->underline) {
            double r, g, b;
            if (block->underline_color) {
                parse_color(block->underline_color, &r, &g, &b);
            } else if (block->fg_color) {
                parse_color(block->fg_color, &r, &g, &b);
            } else {
                r = g = b = 1.0;
            }

            cairo_set_source_rgb(bar->cairo, r, g, b);
            cairo_set_line_width(bar->cairo, bar->config->underline_thickness);
            cairo_move_to(bar->cairo, x,
                bar->height - bar->config->margin_bottom - bar->config->underline_thickness);
            cairo_line_to(bar->cairo, x + width,
                bar->height - bar->config->margin_bottom - bar->config->underline_thickness);
            cairo_stroke(bar->cairo);
        }

        x += width + bar->config->padding * 2;

        // Draw separator if not last block
        if (i < num_blocks - 1 && bar->config->separator) {
            cairo_set_source_rgb(bar->cairo, 1.0, 1.0, 1.0);  // Separator color
            PangoFontDescription *sep_font = pango_font_description_from_string(bar->fonts[0]);
            pango_layout_set_font_description(bar->pango_layout, sep_font);
            pango_layout_set_text(bar->pango_layout, bar->config->separator, -1);
            cairo_move_to(bar->cairo, x - bar->config->padding, y);
            pango_cairo_show_layout(bar->cairo, bar->pango_layout);
            pango_font_description_free(sep_font);
        }

        pango_font_description_free(font_desc);
    }

    free(block_dims);

    // Commit the surface
    wl_surface_attach(bar->surface, bar->buffer, 0, 0);
    wl_surface_damage_buffer(bar->surface, 0, 0, bar->width, bar->height);
    wl_surface_commit(bar->surface);

    printf("Drawing completed\n");
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

static void read_stdin(struct limebar *bar) {
    char buffer[1024];
    ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';

        // Remove trailing newline if present
        if (buffer[bytes_read - 1] == '\n') {
            buffer[bytes_read - 1] = '\0';
        }

        // Free old blocks
        struct text_block *block = bar->blocks;
        while (block) {
            struct text_block *next = block->next;
            free(block->text);
            free(block->fg_color);
            free(block->bg_color);
            free(block->underline_color);
            free(block);
            block = next;
        }
        bar->blocks = NULL;

        if (bar->config->raw_mode) {
            // Create a single block with the raw text
            struct text_block *block = calloc(1, sizeof(struct text_block));
            block->text = strdup(buffer);
            block->fg_color = bar->config->text_color ? strdup(bar->config->text_color) : NULL;
            block->font_index = 0;
            bar->blocks = block;
        } else {
            // Parse input as blocks
            bar->blocks = parse_input(buffer);
        }

        // Redraw
        draw(bar);
    }
}

int main(int argc, char *argv[]) {
    struct bar_config config = {
        .width = 1920,
        .height = 24,
        .background_color = "#1a1a1a",
        .fonts = NULL,
        .num_fonts = 0,
        .underline_thickness = 2,
        .padding = 10,
        .default_alignment = ALIGN_DEFAULT_LEFT,
        .position = POSITION_TOP,
        .margin_top = 0,
        .margin_bottom = 0,
        .margin_left = 0,
        .margin_right = 0,
        .separator = NULL,
        .opacity = 1.0,
        .raw_mode = false,
        .text_color = "#ffffff",  // Default to white text
    };

    static struct option long_options[] = {
        {"geometry", required_argument, 0, 'g'},
        {"background", required_argument, 0, 'B'},
        {"font", required_argument, 0, 'f'},
        {"underline", required_argument, 0, 'u'},
        {"padding", required_argument, 0, 'p'},
        {"alignment", required_argument, 0, 'a'},
        {"position", required_argument, 0, 't'},
        {"margin", required_argument, 0, 'm'},
        {"separator", required_argument, 0, 's'},
        {"opacity", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {"raw", no_argument, 0, 'r'},
                {"text-color", required_argument, 0, 'F'},
                {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "g:B:f:u:p:a:t:m:s:o:rF:h", long_options, NULL)) != -1) {
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
            case 'r':
                            config.raw_mode = true;
                            break;
                        case 'F':
                            config.text_color = optarg;
                            break;
            case 'u':
                config.underline_thickness = atoi(optarg);
                break;
            case 'p':
                config.padding = atoi(optarg);
                break;
            case 'a':
                if (strcmp(optarg, "center") == 0)
                    config.default_alignment = ALIGN_DEFAULT_CENTER;
                else if (strcmp(optarg, "right") == 0)
                    config.default_alignment = ALIGN_DEFAULT_RIGHT;
                break;
            case 't':
                if (strcmp(optarg, "bottom") == 0)
                    config.position = POSITION_BOTTOM;
                break;
            case 'm': {
                int top, right, bottom, left;
                sscanf(optarg, "%d,%d,%d,%d", &top, &right, &bottom, &left);
                config.margin_top = top;
                config.margin_right = right;
                config.margin_bottom = bottom;
                config.margin_left = left;
                break;
            }
            case 's':
                config.separator = strdup(optarg);
                break;
            case 'o':
                config.opacity = atof(optarg);
                if (config.opacity < 0.0) config.opacity = 0.0;
                if (config.opacity > 1.0) config.opacity = 1.0;
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
    bar.config = &config;  // Set the config pointer

    // Set background color
    parse_color_str(config.background_color,
        &bar.bg_color.r,
        &bar.bg_color.g,
        &bar.bg_color.b,
        &bar.bg_color.a);

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

        // Set exclusive zone to reserve space
        zwlr_layer_surface_v1_set_exclusive_zone(bar.layer_surface, bar.height);

        // Set size
        zwlr_layer_surface_v1_set_size(bar.layer_surface, bar.width, bar.height);

        // Set anchor based on position
        uint32_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        if (bar.config->position == POSITION_BOTTOM) {
            anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
        } else {
            anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
        }
        zwlr_layer_surface_v1_set_anchor(bar.layer_surface, anchor);
    zwlr_layer_surface_v1_add_listener(bar.layer_surface,
            &layer_surface_listener, &bar);

    // Commit the surface and wait for the configure event
    wl_surface_commit(bar.surface);
    wl_display_roundtrip(bar.display);

    // Set up polling
    struct pollfd fds[2] = {
        {.fd = STDIN_FILENO, .events = POLLIN},
        {.fd = wl_display_get_fd(bar.display), .events = POLLIN}
    };

    // Main event loop with polling
    while (1) {
        wl_display_flush(bar.display);
        if (poll(fds, 2, -1) > 0) {
            if (fds[0].revents & POLLIN) {
                read_stdin(&bar);
            }
            if (fds[1].revents & POLLIN) {
                if (wl_display_dispatch(bar.display) < 0) {
                    break;
                }
            }
        }
    }

    // Cleanup
    if (config.separator) free(config.separator);
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
