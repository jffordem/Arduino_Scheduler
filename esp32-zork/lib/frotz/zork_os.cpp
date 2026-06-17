/*
 * zork_os.cpp — frotz OS interface for ESP32-S3 WebSocket terminal.
 *
 * Implements all os_* functions declared in frotz.h, with signatures taken
 * directly from the frotz.h in lib/frotz/common/. Also provides the global
 * variables normally defined in frotz's main.c (excluded via library.json).
 */

#include "zork_frotz.h"
#include <Arduino.h>
#include <esp_system.h>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <cstdlib>      /* EXIT_SUCCESS */

/* ── Global state (our interface) ───────────────────────────────────────── */

const char*        zork_story_path  = nullptr;
QueueHandle_t      zork_input_q     = nullptr;
volatile uint32_t  zork_client_id   = 0;
TaskHandle_t       zork_task_handle = nullptr;
ZorkSendFn         zork_send_fn     = nullptr;

/* ── Globals normally defined in frotz's main.c ─────────────────────────── */
/*
 * main.c is excluded via library.json because it contains main() which
 * conflicts with Arduino's main(). We define its globals here instead.
 */

extern "C" {

bool  need_newline_at_exit = FALSE;
char *story_name           = nullptr;
long  story_size           = 0;

enum story story_id = UNKNOWN;

zword  stack[STACK_SIZE];
zword *sp          = nullptr;
zword *fp          = nullptr;
zword  frame_count = 0;

bool ostream_screen = TRUE;
bool ostream_script = FALSE;
bool ostream_memory = FALSE;
bool ostream_record = FALSE;
bool istream_replay = FALSE;
bool message        = FALSE;

int cwin          = 0;
int mwin          = 0;
int mouse_x       = 0;
int mouse_y       = 0;
int menu_selected = 0;
int mouse_button  = 0;

bool enable_wrapping  = FALSE;
bool enable_scripting = FALSE;
bool enable_scrolling = FALSE;
bool enable_buffering = FALSE;

int   option_sound      = 1;
char *option_zcode_path = nullptr;
long  reserve_mem       = 0;

/* Used by getopt.c (excluded) — define stubs so other code links. */
int   zoptind = 1;
int   zoptopt = 0;
char *zoptarg = nullptr;

/* Build info stub. */
const char build_timestamp[] = "esp32-build";

/* --- Opcode stubs from excluded main.c --- */

/* z_piracy: Z-machine opcode that checks if the game is legitimate.
 * We always say it's legal (branch = true). */
void z_piracy(void) {
    branch(!f_setup.piracy);
}

} /* extern "C" */

/* ── Output buffer ──────────────────────────────────────────────────────── */

static char   out_buf[ZORK_OUT_MAX];
static size_t out_len = 0;

static void out_flush() {
    if (out_len == 0 || !zork_send_fn || zork_client_id == 0) {
        out_len = 0;
        return;
    }
    out_buf[out_len] = '\0';
    zork_send_fn(zork_client_id, out_buf, out_len);
    out_len = 0;
}

static void out_char(char c) {
    if (out_len >= ZORK_OUT_MAX - 2) out_flush();
    out_buf[out_len++] = c;
}

/* ── os_* implementations ───────────────────────────────────────────────── */

extern "C" {

/* --- Init / teardown --- */

void os_init_setup(void) {
    f_setup.err_report_mode    = ERR_REPORT_ONCE;
    f_setup.interpreter_number = INTERP_MSDOS;
    f_setup.script_cols        = 80;
    f_setup.sound              = 1;
}

void os_process_arguments(int /*argc*/, char * /*argv*/[]) {
    /* Story path is set via zork_story_path before launching the task. */
    if (zork_story_path)
        f_setup.story_file = const_cast<char*>(zork_story_path);
}

void os_init_screen(void) {
    /*
     * Tell the Z-machine what our terminal looks like.
     * screen_rows/cols: Z-machine header bytes used by V1-V4 games.
     * screen_height/width: used by frotz internally for all versions
     *   (screen.c uses screen_height to size windows regardless of version).
     * 255 rows = scrolling terminal (no [MORE] paging).
     */
    z_header.interpreter_number  = INTERP_MSDOS;
    z_header.interpreter_version = 'F';
    z_header.screen_rows         = 255;
    z_header.screen_cols         = 80;
    z_header.screen_height       = 255;
    z_header.screen_width        = 80;
    z_header.standard_high       = 1;
    z_header.standard_low        = 0;
    if (z_header.version >= V5) {
        z_header.font_width  = 1;
        z_header.font_height = 1;
    }
}

void os_reset_screen(void) {
    out_flush();
}

/* --- Story file I/O --- */

FILE *os_load_story(void) {
    if (!zork_story_path) {
        os_fatal("No story path set.");
        return nullptr;
    }
    Serial.printf("[FROTZ] fopen(%s)\n", zork_story_path);
    FILE *fp = fopen(zork_story_path, "rb");
    if (!fp) os_fatal("Cannot open story file: %s", zork_story_path);
    return fp;
}

int os_storyfile_seek(FILE *fp, long offset, int whence) {
    return fseek(fp, offset, whence);
}

int os_storyfile_tell(FILE *fp) {
    return (int)ftell(fp);
}

/* --- Output --- */

void os_display_char(zchar c) {
    if (cwin != 0) return;              /* suppress status bar (window 1) */
    if (c == '\r')      { out_char('\n'); return; }  /* ZC_RETURN */
    if (c == ZC_INDENT) { out_char('\t'); return; }  /* 0x09 = TAB */
    if (c == ZC_GAP)    { out_char(' ');  return; }  /* 0x0B = VT → space */
    /* Only emit printable ASCII; drop ZC_NEW_STYLE, ZC_NEW_FONT, etc. */
    if (c >= ZC_ASCII_MIN && c <= ZC_ASCII_MAX) out_char((char)c);
}

void os_display_string(const zchar *s) {
    while (*s) os_display_char(*s++);
}

/* --- Input --- */

zchar os_read_line(int max, zchar *buf, int timeout, int /*width*/, int /*continued*/) {
    out_flush();

    char line[ZORK_INPUT_MAX];
    TickType_t ticks = timeout
        ? pdMS_TO_TICKS((uint32_t)timeout * 100)
        : portMAX_DELAY;

    if (xQueueReceive(zork_input_q, line, ticks) == pdTRUE) {
        int n = (int)strlen(line);
        if (n > max) n = max;
        for (int i = 0; i < n; i++) buf[i] = (zchar)(unsigned char)line[i];
        buf[n] = 0;
        return ZC_RETURN;
    }
    return ZC_TIME_OUT;
}

zchar os_read_key(int timeout, bool /*cursor*/) {
    out_flush();

    char line[ZORK_INPUT_MAX];
    TickType_t ticks = timeout
        ? pdMS_TO_TICKS((uint32_t)timeout * 100)
        : portMAX_DELAY;

    if (xQueueReceive(zork_input_q, line, ticks) == pdTRUE && line[0])
        return (zchar)(unsigned char)line[0];
    return ZC_TIME_OUT;
}

char *os_read_file_name(const char * /*default_name*/, int /*flag*/) {
    /* Save/restore not supported in v1. */
    return nullptr;
}

/* --- Fatal / warning / quit --- */

void os_fatal(const char *msg, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);

    Serial.printf("[FROTZ FATAL] %s\n", buf);
    out_len = 0;
    const char prefix[] = "\r\n[Fatal: ";
    size_t plen = sizeof(prefix) - 1;
    memcpy(out_buf, prefix, plen);
    out_len = plen;
    size_t mlen = strlen(buf);
    if (out_len + mlen + 3 < ZORK_OUT_MAX) {
        memcpy(out_buf + out_len, buf, mlen);
        out_len += mlen;
        out_buf[out_len++] = ']';
        out_buf[out_len++] = '\n';
    }
    out_flush();
    zork_client_id = 0;
    zork_task_handle = nullptr;
    vTaskDelete(nullptr);
    for (;;) {}
}

void os_warn(const char *msg, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);
    Serial.printf("[FROTZ WARN] %s\n", buf);
}

void os_quit(int /*status*/) {
    out_flush();
    static const char bye[] = "\r\n[Game ended. Refresh to play again.]\r\n";
    if (zork_send_fn && zork_client_id)
        zork_send_fn(zork_client_id, bye, sizeof(bye) - 1);
    zork_client_id = 0;
    zork_task_handle = nullptr;
    vTaskDelete(nullptr);
    for (;;) {}
}

/* --- Metrics --- */

int  os_char_width(zchar /*c*/)          { return 1; }
int  os_string_width(const zchar *s)     { int n=0; while(*s++) n++; return n; }
int  os_random_seed(void)                { return (int)(esp_random() & 0x7FFFFFFF); }
int  os_check_unicode(int /*mode*/, zchar /*c*/) { return 0; }

/* --- Colour / style (no-ops for dumb terminal) --- */

void os_set_colour(int /*fg*/, int /*bg*/) {}

/* Two complementary newline sources:
 *
 * 1. os_set_cursor: fires for the "cursor advance" path in screen_new_line
 *    (when y_cursor is not yet at the bottom of the window).  Emit '\n'
 *    only for an EXACT 1-row increase so that large jumps from reset_cursor
 *    (which repositions to the bottom row) don't produce a flood of blanks.
 *
 * 2. os_scroll_area: fires for the "scroll" path in screen_new_line (when
 *    y_cursor is pinned at the bottom — which is always true once the game
 *    has called split_window and reset_cursor moves the cursor there).
 *    This handles all normal newlines during gameplay. */

static int tracked_row = -1;

void os_set_cursor(int row, int /*col*/) {
    if (cwin != 0) { tracked_row = -1; return; }
    if (tracked_row < 0) { tracked_row = row; return; }
    if (row == tracked_row + 1) out_char('\n');  /* exact 1-row advance */
    tracked_row = row;
}
void os_set_font(int /*font*/)                       {}
void os_set_text_style(int /*style*/)                {}
int  os_get_text_style(void)                         { return 0; }
int  os_peek_colour(void)                            { return 0; }
int  os_from_true_colour(zword /*c*/)                { return 0; }
zword os_to_true_colour(int /*c*/)                   { return 0; }

/* --- Prompts / timing --- */

void os_more_prompt(void) {
    out_flush();
    /* Dumb scrolling terminal — ignore MORE pauses. */
}

void os_beep(int /*style*/)         {}
void os_restart_game(int /*stage*/) {}
void os_tick(void)                  {}

/* --- Screen geometry (no resizing) --- */

void os_scroll_area(int /*top*/, int /*bot*/, int /*left*/, int /*right*/, int /*u*/) {
    /* Each call represents one line scrolled up in the main window. */
    if (cwin == 0) out_char('\n');
}
void os_erase_area(int /*top*/, int /*bot*/, int /*left*/, int /*right*/, int /*bg*/)  {}
bool os_repaint_window(int /*win*/, int /*ypos_old*/, int /*ypos_new*/,
                       int /*xpos*/, int /*ysize*/, int /*xsize*/)                     { return false; }

/* --- Graphics (no-ops) --- */

void os_draw_picture(int /*pic*/, int /*y*/, int /*x*/)          {}
bool os_picture_data(int /*pic*/, int * /*h*/, int * /*w*/)      { return false; }
int  os_font_data(int /*font*/, int * /*h*/, int * /*w*/)        { return 0; }

/* --- Sound (no-ops) --- */

void os_init_sound(void)                                          {}
void os_prepare_sample(int /*num*/)                               {}
void os_start_sample(int /*n*/, int /*vol*/, int /*rep*/, zword /*eos*/) {}
void os_stop_sample(int /*n*/)                                    {}
void os_finish_with_sample(int /*n*/)                             {}

} /* extern "C" */

/* ── Interpreter task (core 1) ───────────────────────────────────────────── */

void zork_interpreter_task(void* /*param*/) {
    Serial.println("[FROTZ] Interpreter task started.");

    if (!zork_story_path || !zork_input_q) {
        Serial.println("[FROTZ] Missing story path or input queue.");
        if (zork_send_fn && zork_client_id)
            zork_send_fn(zork_client_id, "\r\nNo story file configured.\r\n", 29);
        zork_client_id = 0;
        zork_task_handle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    Serial.printf("[FROTZ] Loading: %s\n", zork_story_path);

    init_header();
    init_setup();
    os_init_setup();
    os_process_arguments(0, nullptr);
    init_buffer();
    init_err();
    init_memory();      /* calls os_load_story() → fopen(zork_story_path) */
    init_process();
    init_sound();
    os_init_screen();
    init_undo();
    z_restart();
    interpret();        /* main game loop */
    reset_screen();
    reset_memory();
    os_reset_screen();
    os_quit(EXIT_SUCCESS);
}
