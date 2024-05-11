#include "event.h"

#include "common/logger.h"
#include "common/asserts.h"
#include "common/containers/darray.h"

typedef struct {
    fp_event_callback *callbacks;
} registered_event_t;

registered_event_t registered_events[EVENT_CODE_COUNT];

b8 event_system_init(void)
{
    return true;
}

void event_system_shutdown(void)
{
    for (i32 i = 0; i < EVENT_CODE_COUNT; i++) {
        if (registered_events[i].callbacks != 0) {
            darray_destroy(registered_events[i].callbacks);
        }
    }
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

    if (registered_events[code].callbacks == 0) {
        LOG_WARN("tried to fire event with no registered callbacks");
        return;
    }

    u64 callbacks_length = darray_length(registered_events[code].callbacks);
    for (u64 i = 0; i < callbacks_length; i++) {
        fp_event_callback callback = registered_events[code].callbacks[i];
        if (callback(code, data))
            break;
    }
}
