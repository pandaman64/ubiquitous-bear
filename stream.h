#ifndef UBIQUITOUS_BEAR_STREAM_HEADER
#define UBIQUITOUS_BEAR_STREAM_HEADER

#include "uv.h"

typedef struct bear_stream_s bear_stream_t;
typedef void (*bear_stream_handler)(char const*,size_t);

void bear_init();
bear_stream_t *create_bear_stream(uv_loop_t *loop,char const* c_key,char const* c_sec,char const* t_key,char const* t_sec); void add_userstream_handle(bear_stream_t *stream,char const* endpoint,bear_stream_handler handler);

#endif
