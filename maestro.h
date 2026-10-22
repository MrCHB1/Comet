/*
 * maestro.h: the Maestro API.
 *
 * The library is installed as maestro.dll, libmaestro.so or libmaestro.dylib,
 * and exports maestro_get_interface(), which lets you reach everything else
 * through the function tables it returns.
 */
/* clang-format off */
/*
 *     const MaestroCoreV1 *core = maestro_get_interface(MAESTRO_CORE_V1);
 *     const MaestroRealtimeV1 *rt = maestro_get_interface(MAESTRO_REALTIME_V1);
 *     if (!core || !rt) {
 *         ... // this library is too old for the app
 *     }
 *
 *     MaestroAppInfo app = { sizeof app };
 *     app.app_id = "org.example.app";
 *     MaestroRealtime *inst;
 *     if (rt->open(&app, NULL, &inst) != MAESTRO_OK) {
 *         puts(core->last_error());
 *         ...
 *     }
 *     rt->send_short(inst, 0, 0x7F3C90, MAESTRO_TIME_NOW);
 *     rt->close(inst);
 */
/* clang-format on */
/*
 * Versioning:
 *  - Each interface has an ID ending in its version, e.g. "maestro.realtime/1".
 *    A released interface never changes: no new functions, no changed
 *    signatures or behavior. Changes ship as a new version beside the old
 *    one ("maestro.realtime/2"), and the library keeps serving the old ones.
 *  - maestro_get_interface() returns NULL for an ID the library doesn't
 *    provide, so an app checks for exactly the features it uses. There is no
 *    global API version to compare.
 *  - Handles are shared between versions: a MaestroRealtime opened through
 *    one version of an interface works with every other interface that takes
 *    a MaestroRealtime.
 *  - Each object has exactly one interface: its table holds everything the
 *    object can do.
 *  - Structs start with a uint32_t struct_size, which the caller sets to
 *    sizeof the struct. New fields are only ever added at the end. The
 *    library treats input fields past struct_size as 0, and doesn't write
 *    output fields past it.
 *  - Enums are passed as uint32_t. The library rejects input values it
 *    doesn't recognize with MAESTRO_ERR_UNSUPPORTED; the app must tolerate
 *    output values (results, notifications) it doesn't recognize.
 *
 * Common conventions:
 *  - Strings are UTF-8 and NUL-terminated, paths included. The library copies
 *    any string it needs to keep past the end of a call.
 *  - Booleans are uint32_t (0 or 1).
 *  - A failing function returns a MaestroResult below 0, and
 *    MaestroCoreV1.last_error() describes it.
 *  - Callbacks may run on any thread. Keep them short and don't call back into
 *    Maestro from inside them.
 *  - Threading is noted on each function:
 *      [rt]   realtime-safe (see "Realtime playback"). Any thread.
 *      [any]  may block. Any thread.
 *      [ctrl] may block. Must not run concurrently with other calls on the
 *      same object.
 *
 * Maestro offers two independent objects, each with its own handle type:
 *  - MaestroRealtime: a live engine with its own audio output.
 *  - MaestroFileRender: one job that renders a MIDI file to an audio file.
 * Both use the user's profile for the app named in MaestroAppInfo, so the
 * soundfonts and synth settings match between playback and rendering.
 */
#ifndef MAESTRO_H
#define MAESTRO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define MAESTRO_CALL __cdecl
#define MAESTRO_API __declspec(dllimport)
#else
#define MAESTRO_CALL
#define MAESTRO_API
#endif

/* ======================================================================== */
/* ---- Entry point ---- */
/* ======================================================================== */

/* Define MAESTRO_NO_PROTOTYPES when loading the library at runtime, resolve
 * "maestro_get_interface" and cast it to MaestroGetInterfaceFn. */
#ifndef MAESTRO_NO_PROTOTYPES
/* [rt] Returns the function table for the interface `id`, or NULL if this
 * library doesn't provide it. The table is constant and stays valid while the
 * library is loaded. */
MAESTRO_API const void *MAESTRO_CALL maestro_get_interface(const char *id);
#endif

typedef const void *(MAESTRO_CALL *MaestroGetInterfaceFn)(const char *id);

/* ======================================================================== */
/* ---- Common ---- */
/* ======================================================================== */

/* Returned by every fallible call. Anything below 0 is an error. Treat a
 * negative value not listed here as a generic failure; positive values are
 * reserved. */
typedef int32_t MaestroResult;
enum {
  MAESTRO_OK = 0,
  MAESTRO_ERR_INVALID_ARG = -1,
  MAESTRO_ERR_UNSUPPORTED = -2, /* a value this library doesn't recognize */
  MAESTRO_ERR_DISABLED =
      -3, /* the user disabled the API, or this application's profile */
  MAESTRO_ERR_INTERNAL = -4, /* includes a panic caught at the FFI boundary */
  MAESTRO_ERR_UNAVAILABLE = -5, /* the synthesizer or the audio device could
                                 * not be started */

  /* Realtime */
  MAESTRO_ERR_NOT_RUNNING = -100, /* the engine is stopped, or restarting for
                                   * a settings change: drop the event or
                                   * retry */

  /* File rendering */
  MAESTRO_ERR_IO = -200,        /* reading the MIDI or writing the output */
  MAESTRO_ERR_BAD_MIDI = -201,  /* the input is not a MIDI file Maestro reads */
  MAESTRO_ERR_ENCODER = -202,   /* the output format could not be encoded */
  MAESTRO_ERR_CANCELLED = -203, /* the job was cancelled */
  MAESTRO_ERR_BUSY = -204       /* the job is already running or finished */
};

/* Severity passed to the log callback. */
typedef uint32_t MaestroLogLevel;
enum { MAESTRO_LOG_ERROR = 0, MAESTRO_LOG_WARN = 1 };

typedef void(MAESTRO_CALL *MaestroLogFn)(void *user, MaestroLogLevel level,
                                         const char *message);

/* Who the app is. Shared by realtime and file rendering. */
typedef struct MaestroAppInfo {
  uint32_t struct_size;
  uint32_t reserved;
  const char *app_id; /* required. Stable reverse-DNS ID, e.g. "org.example.app"
                       * ([A-Za-z0-9._-]). Selects the user's profile */
  const char *app_name; /* shown in Maestro's settings; may be NULL */
  MaestroLogFn log;     /* may be NULL */
  void *user;           /* passed to log, and to the object's own callbacks */
} MaestroAppInfo;

/* ---- Interface: core (since Maestro 0.2) ---- */
#define MAESTRO_CORE_V1 "maestro.core/1"
typedef struct MaestroCoreV1 {
  /* [rt] Version of the Maestro release this library comes from, as
   * MAESTRO_MAKE_VERSION. For display and bug reports only: check features
   * with maestro_get_interface(). */
  uint32_t(MAESTRO_CALL *get_version)(void);
  /* [rt] Description of the last error on the calling thread. Valid until this
   * thread's next Maestro call. Never NULL. */
  const char *(MAESTRO_CALL *last_error)(void);
} MaestroCoreV1;

#define MAESTRO_MAKE_VERSION(major, minor)                                     \
  (((uint32_t)(major) << 16) | (uint32_t)(minor))

/* ======================================================================== */
/* ---- Realtime playback ---- */
/* ======================================================================== */
/*
 * Conventions:
 *  - Timestamps use the platform's native monotonic clock:
 *    QueryPerformanceCounter ticks on Windows, CLOCK_MONOTONIC nanoseconds on
 *    Linux, mach_absolute_time ticks on macOS. A timestamp of 0 (or one in the
 *    past) plays as soon as possible. Timestamps within an engine should not
 *    decrease; an earlier one plays no sooner than the events already due.
 *  - [rt] means no I/O, and locks are only held briefly, shared with other
 *    senders and the render thread; stats calls also wait out an engine
 *    rebuild after a settings change. Memory is allocated only while event
 *    queues grow to their working size, and for SysEx.
 */

typedef uint64_t MaestroTime;
#define MAESTRO_TIME_NOW ((MaestroTime)0)

typedef struct MaestroRealtime MaestroRealtime; /* opaque */

/* What the notify callback reports. */
typedef uint32_t MaestroNotify;
enum {
  MAESTRO_NOTIFY_SETTINGS_CHANGED =
      0, /* the user edited this app's profile; call get_info again */
  MAESTRO_NOTIFY_ENGINE_STOPPED = 1, /* the engine failed to restart */
  MAESTRO_NOTIFY_ENGINE_STARTED = 2,
  MAESTRO_NOTIFY_ENGINE_DISABLED =
      3 /* the user disabled the API, or this application's profile */
};

typedef void(MAESTRO_CALL *MaestroNotifyFn)(void *user, MaestroNotify what);

/* What the app would prefer. 0 means "no preference". */
typedef struct MaestroRealtimeParams {
  uint32_t struct_size;
  uint32_t num_ports; /* MIDI 1 ports the app will address, 1-16
                       * (default 1) */
  uint32_t midi2;     /* 1 gives the app all 16 UMP groups as ports */
  uint32_t reserved;
  MaestroNotifyFn notify; /* may be NULL */
} MaestroRealtimeParams;

/* The settings that resulted from the params combined with the user's
 * profile. */
typedef struct MaestroRealtimeInfo {
  uint32_t struct_size; /* set by the caller */
  uint32_t sample_rate;
  uint32_t num_ports;
  uint32_t midi2;
  uint32_t render_block_us;
  uint32_t max_schedule_us; /* how far ahead of now an event may be scheduled;
                             * later ones play at that limit instead */
} MaestroRealtimeInfo;

typedef struct MaestroRealtimeStats {
  uint32_t struct_size; /* set by the caller */
  float render_time;    /* render load: 100 is the whole time budget of a
                         * block, and above it the audio falls behind */
  uint32_t latency_us;  /* timestamp to audible; 0 until audio starts */
  uint32_t reserved;
  uint64_t voice_count; /* number of current active voices */
  uint64_t nps;         /* notes per second processed by the synth */
  uint64_t eps;         /* events per second processed by the synth */
  uint64_t nps_limit;   /* the current limit, after the load limiter */
} MaestroRealtimeStats;

/* ---- Interface: realtime engine (since Maestro 0.2) ---- */
#define MAESTRO_REALTIME_V1 "maestro.realtime/1"
typedef struct MaestroRealtimeV1 {
  /* [any] Opens a live engine with its own audio output. A process can have
   * several open at once. params may be NULL. */
  MaestroResult(MAESTRO_CALL *open)(const MaestroAppInfo *app,
                                    const MaestroRealtimeParams *params,
                                    MaestroRealtime **out);
  /* [ctrl] Closes the engine. After this returns, no callback for it will run
   * again. */
  void(MAESTRO_CALL *close)(MaestroRealtime *rt);

  /* [rt] The current host time. */
  MaestroTime(MAESTRO_CALL *now)(void);
  /* [rt] Converts a duration for scheduling ahead ("now + 5 ms"). */
  MaestroTime(MAESTRO_CALL *ns_to_ticks)(uint64_t ns);

  /* [rt] MIDI 1.0: a packed short message, status | data1 << 8 |
   * data2 << 16. */
  MaestroResult(MAESTRO_CALL *send_short)(MaestroRealtime *rt, uint8_t port,
                                          uint32_t msg, MaestroTime at);
  /* [rt] MIDI 1.0: a byte stream of SysEx, or any number of messages, running
   * status included. All of them share `at`, and sending many at once is much
   * cheaper per message than send_short. A message cut off at the end of
   * `data` is finished by the next call. The data is copied before the call
   * returns. */
  MaestroResult(MAESTRO_CALL *send_long)(MaestroRealtime *rt, uint8_t port,
                                         const uint8_t *data, size_t len,
                                         MaestroTime at);
  /* [rt] One or more complete Universal MIDI Packets, in host byte order.
   * Packets for groups at or above MaestroRealtimeInfo.num_ports are
   * dropped. */
  MaestroResult(MAESTRO_CALL *send_ump)(MaestroRealtime *rt,
                                        const uint32_t *words, size_t count,
                                        MaestroTime at);

  /* [any] The effective settings. */
  MaestroResult(MAESTRO_CALL *get_info)(MaestroRealtime *rt,
                                        MaestroRealtimeInfo *out);
  /* [rt] */
  MaestroResult(MAESTRO_CALL *get_stats)(MaestroRealtime *rt,
                                         MaestroRealtimeStats *out);

  /* [any] All notes off, controllers reset, and every queued event dropped. */
  MaestroResult(MAESTRO_CALL *reset)(MaestroRealtime *rt);
  /* [rt] Silences every port now. Controllers, programs and queued events
   * are kept, so it suits a seek or stop that should not reset the song. */
  MaestroResult(MAESTRO_CALL *notes_off)(MaestroRealtime *rt);

  /* [any] Opens Maestro's settings for this app's profile so the user can
   * tune it. */
  MaestroResult(MAESTRO_CALL *show_settings)(MaestroRealtime *rt);
} MaestroRealtimeV1;

/* ======================================================================== */
/* ---- File rendering ---- */
/* ======================================================================== */
/*
 * With `app` filled in as for realtime:
 *
 *     const MaestroFileRenderV1 *fr =
 *         maestro_get_interface(MAESTRO_FILE_RENDER_V1);
 *     MaestroFileRenderParams params = { sizeof params };
 *     params.midi_path = "song.mid";
 *     params.output_path = "song.flac";
 *     params.format = MAESTRO_FORMAT_FLAC;
 *     MaestroFileRender *job;
 *     if (fr->open(&app, &params, &job) != MAESTRO_OK) {
 *         ...
 *     }
 *     MaestroResult r = fr->run(job); // blocks
 *     fr->close(job);
 *
 * A job renders once: open, run, close. Rendering runs as fast as the machine
 * allows and doesn't touch the audio device or the host clock. Call progress()
 * and cancel() from another thread while run() is blocking.
 */

typedef struct MaestroFileRender MaestroFileRender; /* opaque */

typedef uint32_t MaestroFormat;
enum {
  MAESTRO_FORMAT_WAV = 0,
  MAESTRO_FORMAT_FLAC = 1,
  MAESTRO_FORMAT_MP3 = 2,
  MAESTRO_FORMAT_OGG = 3
};

/* How the file's tracks map onto synth ports. Either way, everything is mixed
 * into the one output file. */
typedef uint32_t MaestroPortMode;
enum {
  MAESTRO_PORTS_MULTI = 0, /* each track plays on the port its MIDI Port meta
                            * event (or UMP group) assigns, for files with
                            * more than 16 channels */
  MAESTRO_PORTS_SINGLE = 1 /* every track plays on one port */
};

/* 0 picks the default. Everything else (SoundFonts, synth and effects
 * settings) comes from the user's profile for the app. */
typedef struct MaestroFileRenderParams {
  uint32_t struct_size;
  uint32_t reserved;
  const char *midi_path;   /* required */
  const char *output_path; /* required. Written only once the render
                            * completes, replacing any file already there */
  MaestroFormat format;    /* default WAV */
  uint32_t wav_bit_depth;  /* 16, 24 or 32 (float, the default). WAV only */
  uint32_t bitrate_kbps;   /* 64, 96, 128, 160, 192 (default), 224, 256 or 320.
                            * MP3 and OGG only */
  uint32_t sample_rate;    /* default: the profile's */
  uint32_t channels;       /* 1 or 2; default: the profile's */
  MaestroPortMode port_mode;
  uint32_t max_ports; /* MAESTRO_PORTS_MULTI only, 1-16; default no limit */
} MaestroFileRenderParams;

typedef struct MaestroFileRenderProgress {
  uint32_t struct_size; /* set by the caller */
  float speed;          /* rendered seconds per wall-clock second */
  double position_s;    /* how much of the song has been rendered */
  double length_s;      /* the song's length, ending with its last event */
  uint64_t notes;       /* notes rendered so far */
  uint64_t events;      /* events rendered so far */
  uint64_t voice_count;
} MaestroFileRenderProgress;

/* ---- Interface: file rendering (since Maestro 0.2) ---- */
#define MAESTRO_FILE_RENDER_V1 "maestro.file_render/1"
typedef struct MaestroFileRenderV1 {
  /* [any] Creates a job and checks its params and the MIDI file. Nothing is
   * rendered or written until run(). */
  MaestroResult(MAESTRO_CALL *open)(const MaestroAppInfo *app,
                                    const MaestroFileRenderParams *params,
                                    MaestroFileRender **out);
  /* [ctrl] Frees the job. Must not be called while run() is in progress. */
  void(MAESTRO_CALL *close)(MaestroFileRender *job);

  /* [any] Renders the whole file, blocking until it is done. Returns
   * MAESTRO_OK, MAESTRO_ERR_CANCELLED, or an error. The output is written
   * beside output_path under a temporary name and moved into place at the
   * end, so a cancelled or failed job leaves nothing behind and keeps any
   * file that was there. Can be called once per job; later calls return
   * MAESTRO_ERR_BUSY. */
  MaestroResult(MAESTRO_CALL *run)(MaestroFileRender *job);
  /* [any] Asks a running job to stop. run() returns MAESTRO_ERR_CANCELLED soon
   * after. Harmless before run() (which then returns at once) or after it. */
  void(MAESTRO_CALL *cancel)(MaestroFileRender *job);
  /* [any] Progress so far. Valid before, during and after run(). */
  MaestroResult(MAESTRO_CALL *progress)(MaestroFileRender *job,
                                        MaestroFileRenderProgress *out);
} MaestroFileRenderV1;

#ifdef __cplusplus
}
#endif
#endif /* MAESTRO_H */
