/*
** Copyright (c) 2017 rxi
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the MIT license. See `cmixer.c` for details.
**
** Ported to plan 9 on 13mar2021
**/

#define CM_VERSION "0.1.1"

#define CLAMP(x, a, b)    ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))
#define MIN(a, b)         ((a) < (b) ? (a) : (b))
#define MAX(a, b)         ((a) > (b) ? (a) : (b))

#define FX_BITS           (12)
#define FX_UNIT           (1 << FX_BITS)
#define FX_MASK           (FX_UNIT - 1)
#define FX_FROM_FLOAT(f)  ((f) * FX_UNIT)
#define FX_LERP(a, b, p)  ((a) + ((((b) - (a)) * (p)) >> FX_BITS))

#define BUFFER_SIZE       (512)
#define BUFFER_MASK       (BUFFER_SIZE - 1)

typedef short           cm_Int16;
typedef int             cm_Int32;
typedef vlong           cm_Int64;
typedef uchar           cm_UInt8;
typedef ushort          cm_UInt16;
typedef ulong           cm_UInt32;


typedef struct {
  int type;
  void *udata;
  char *msg;
  cm_Int16 *buffer;
  int length;
} cm_Event;

typedef void (*cm_EventHandler)(cm_Event *e);

typedef struct {
  cm_EventHandler handler;
  void *udata;
  int samplerate;
  int length;
} cm_SourceInfo;


enum {
  CM_STATE_STOPPED,
  CM_STATE_PLAYING,
  CM_STATE_PAUSED
};

enum {
  CM_EVENT_LOCK,
  CM_EVENT_UNLOCK,
  CM_EVENT_DESTROY,
  CM_EVENT_SAMPLES,
  CM_EVENT_REWIND
};

typedef struct cm_Source cm_Source;
struct cm_Source {
  cm_Source *next;       /* Next source in list */
  cm_Int16 buffer[BUFFER_SIZE]; /* Internal buffer with raw stereo PCM */
  cm_EventHandler handler;      /* Event handler */
  void *udata;          /* Stream's udata (from cm_SourceInfo) */
  int samplerate;       /* Stream's native samplerate */
  int length;           /* Stream's length in frames */
  int end;              /* End index for the current play-through */
  int state;            /* Current state (playing|paused|stopped) */
  cm_Int64 position;    /* Current playhead position (fixed point) */
  int lgain, rgain;     /* Left and right gain (fixed point) */
  int rate;             /* Playback rate (fixed point) */
  int nextfill;         /* Next frame idx where the buffer needs to be filled */
  int loop;             /* Whether the source will loop when `end` is reached */
  int rewind;           /* Whether the source will rewind before playing */
  int active;           /* Whether the source is part of `sources` list */
  double gain;          /* Gain set by `cm_set_gain()` */
  double pan;           /* Pan set by `cm_set_pan()` */
};


char* cm_get_error(void);
void cm_init(int);
void cm_set_lock(cm_EventHandler);
void cm_set_master_gain(double);
void cm_process(cm_Int16*, int);

cm_Source* cm_new_source(cm_SourceInfo*);
cm_Source* cm_new_source_from_file(char *);
cm_Source* cm_new_source_from_mem(void *, int);
void cm_destroy_source(cm_Source *);
double cm_get_length(cm_Source *);
double cm_get_position(cm_Source *);
int cm_get_state(cm_Source *);
void cm_set_gain(cm_Source *, double);
void cm_set_pan(cm_Source *, double);
void cm_set_pitch(cm_Source *, double);
void cm_set_loop(cm_Source *, int);
void cm_play(cm_Source *);
void cm_pause(cm_Source *);
void cm_stop(cm_Source *);
