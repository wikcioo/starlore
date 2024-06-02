#include "event.h"

#include <pthread.h>

#include "common/logger.h"
#include "common/asserts.h"
#include "common/containers/darray.h"
#include "common/containers/ring_buffer.h"

#define MAX_POLL_EVENTS 32

typedef struct {
    fp_event_callback *callbacks;
} registered_event_t;

typedef struct {
    event_code_e code;
    event_data_t data;
} event_t;

static registered_event_t registered_events[EVENT_CODE_COUNT];
static void *event_queue;
static pthread_mutex_t event_queue_mutex;

b8 event_system_init(void)
{
    event_queue = ring_buffer_reserve(256, sizeof(event_t));
    if (pthread_mutex_init(&event_queue_mutex, NULL) != 0) {
        LOG_ERROR("failed to initialize event queue mutex");
        return false;
    }
    return true;
}

void event_system_shutdown(void)
{
    for (i32 i = 0; i < EVENT_CODE_COUNT; i++) {
        if (registered_events[i].callbacks != 0) {
            darray_destroy(registered_events[i].callbacks);
        }
    }

    ring_buffer_destroy(event_queue);
    pthread_mutex_destroy(&event_queue_mutex);
}

void event_system_register(event_code_e code, fp_event_callback callback)
{
    ASSERT(code > EVENT_CODE_NONE && code < EVENT_CODE_COUNT);
    ASSERT(callback);

    if (registered_events[code].callbacks == 0) {
        registered_events[code].callbacks = darray_create(sizeof(registered_event_t));
    }

    darray_push(registered_events[code].callbacks, callback);
}

void event_system_unregister(event_code_e code, fp_event_callback callback)
{
    ASSERT(code > EVENT_CODE_NONE && code < EVENT_CODE_COUNT);
    ASSERT(callback);

    u64 callbacks_length = darray_length(registered_events[code].callbacks);
    for (u64 i = 0; i < callbacks_length; i++) {
        if (registered_events[code].callbacks[i] == callback) {
            darray_pop_at(registered_events[code].callbacks, i, 0);
            return;
        }
    }

    LOG_WARN("failed to find callback to unregister: code %d, callback %p", code, callback);
}

void event_system_fire(event_code_e code, event_data_t data)
{
    ASSERT(code > EVENT_CODE_NONE && code < EVENT_CODE_COUNT);

    b8 status = false;
    event_t event = { .code = code, .data = data };

    pthread_mutex_lock(&event_queue_mutex);
    ring_buffer_enqueue(event_queue, event, &status);
    pthread_mutex_unlock(&event_queue_mutex);

    if (!status) {
        LOG_WARN("failed to enqueue new event: code=%d", code);
    }
}

void event_system_poll_events(void)
{
    u64 counter = 0;
    u64 length;
    while ((length = ring_buffer_length(event_queue)) > 0 && counter < MAX_POLL_EVENTS) {
        b8 status;
        event_t event;
        pthread_mutex_lock(&event_queue_mutex);
        ring_buffer_dequeue(event_queue, &event, &status);
        pthread_mutex_unlock(&event_queue_mutex);

        if (!status) {
            LOG_WARN("failed to dequeue event from the queue: length=%lu", length);
            break;
        }

        registered_event_t *re = &registered_events[event.code];
        if (re->callbacks == 0) {
            // no registered callbacks
            continue;
        }

        u64 callbacks_length = darray_length(re->callbacks);
        for (u64 i = 0; i < callbacks_length; i++) {
            fp_event_callback callback = re->callbacks[i];
            if (callback(event.code, event.data)) {
                // handled
                break;
            }
        }

        counter++;
    }
}
