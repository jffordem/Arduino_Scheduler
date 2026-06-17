#pragma once

/*
 * Shared interface between main.cpp (WiFi/WebSocket) and zork_os.cpp (frotz).
 *
 * frotz common/ sources are plain C; include them with extern "C" in C++ files.
 *
 * __UNIX_PORT_FILE must be defined before including frotz.h from C++ to prevent
 * frotz.h from doing "typedef int bool", which conflicts with C++'s built-in bool.
 */

#ifdef __cplusplus
#  ifndef __UNIX_PORT_FILE
#    define __UNIX_PORT_FILE
#    define ZORK_UNDEF_UNIX_PORT_FILE
#  endif
extern "C" {
#endif

#include "common/frotz.h"

#ifdef __cplusplus
#  ifdef ZORK_UNDEF_UNIX_PORT_FILE
#    undef __UNIX_PORT_FILE
#    undef ZORK_UNDEF_UNIX_PORT_FILE
#  endif
/* frotz.h defines "runtime_error" as a macro, which clobbers std::runtime_error
 * when C++ standard library headers are included after this. Undef it now so C++
 * callers can safely use <stdexcept>. The frotz C files include frotz.h directly
 * and are unaffected. */
#  ifdef runtime_error
#    undef runtime_error
#  endif
#  ifdef runtime_error_repeat
#    undef runtime_error_repeat
#  endif
#endif

/* frotz internal startup sequence (defined in common/*.c) */
void init_header(void);
void init_setup(void);
void init_buffer(void);
void init_err(void);
void init_memory(void);
void init_process(void);
void init_sound(void);
void init_undo(void);
void z_restart(void);
void interpret(void);
void reset_screen(void);
void reset_memory(void);

#ifdef __cplusplus
}
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

/* Max bytes in one player input line (Z-machine spec max is 255). */
#define ZORK_INPUT_MAX  256

/* Output accumulation buffer: flushed to WebSocket on each os_read_line(). */
#define ZORK_OUT_MAX   4096

/*
 * Globals defined in zork_os.cpp, set by main.cpp before launching the task.
 *
 * zork_story_path : VFS path to the story file, e.g. "/littlefs/zork1.z5"
 * zork_input_q   : FreeRTOS queue; items are char[ZORK_INPUT_MAX] text lines
 * zork_client_id : active WebSocket client ID (0 = no player)
 * zork_task_handle: handle of the running interpreter task (NULL if idle)
 */
extern const char*        zork_story_path;
extern QueueHandle_t      zork_input_q;
extern volatile uint32_t  zork_client_id;
extern TaskHandle_t       zork_task_handle;

/*
 * Output callback registered by main.cpp.
 * zork_os.cpp calls this to push interpreter output to the WebSocket client.
 * `text` is always null-terminated; `len` is provided for convenience.
 */
typedef void (*ZorkSendFn)(uint32_t clientId, const char* text, size_t len);
extern ZorkSendFn zork_send_fn;

/* Entry point for the FreeRTOS interpreter task — pinned to core 1. */
void zork_interpreter_task(void* param);
