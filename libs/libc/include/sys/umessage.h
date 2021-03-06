#ifndef _SYS_UMESSAGE_H
#define _SYS_UMESSAGE_H 1

#include <net/if.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

#include <kernel/hal/input.h>
#include <kernel/net/link_layer_address.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum umessage_category_number {
    UMESSAGE_INTERFACE,
    UMESSAGE_INPUT,
    UMESSAGE_NUM_CATEGORIES,
};

struct umessage {
    uint32_t length;
    uint16_t category;
    uint16_t type;
};
#define UMESSAGE_DATA(u)       ((void*) ((u) + 1))
#define UMESSAGE_VALID(u, len) ((len) >= sizeof(struct umessage) && (u)->length == (len) && (u)->category < UMESSAGE_NUM_CATEGORIES)

enum umessage_interface_request_type {
    UMESSAGE_INTERFACE_LIST_REQUEST,
    UMESSAGE_INTERFACE_SET_STATE_REQUEST,
    UMESSAGE_INTERFACE_NUM_REQUESTS,
};
#define UMESSAGE_INTERFACE_REQUEST_VALID(u, len) \
    (UMESSAGE_VALID(u, len) && (u)->category == UMESSAGE_INTERFACE && (u)->type < UMESSAGE_INTERFACE_NUM_REQUESTS)

enum umessage_interface_message_type {
    UMESSAGE_INTERFACE_LIST,
    UMESSAGE_INTERFACE_NUM_MESSAGES,
};
#define UMESSAGE_INTERFACE_MESSAGE_VALID(u, len) \
    (UMESSAGE_VALID(u, len) && (u)->category == UMESSAGE_INTERFACE && (u)->type < UMESSAGE_INTERFACE_NUM_MESSAGES)

struct umessage_interface_list_request {
    struct umessage base;
};
#define UMESSAGE_INTERFACE_LIST_REQUEST_VALID(u, len)                                                       \
    (UMESSAGE_INTERFACE_REQUEST_VALID(u, len) && (len) >= sizeof(struct umessage_interface_list_request) && \
     (u)->type == UMESSAGE_INTERFACE_LIST_REQUEST)

struct umessage_interface_set_state_request {
    struct umessage base;
    int interface_index;
    bool set_default_gateway;
    bool set_address;
    bool set_subnet_mask;
    bool set_flags;
    struct in_addr default_gateway;
    struct in_addr address;
    struct in_addr subnet_mask;
    int flags;
};
#define UMESSAGE_INTERFACE_SET_STATE_REQUEST_VALID(u, len)                                                       \
    (UMESSAGE_INTERFACE_REQUEST_VALID(u, len) && (len) >= sizeof(struct umessage_interface_set_state_request) && \
     (u)->type == UMESSAGE_INTERFACE_SET_STATE_REQUEST)

struct umessage_interface_desc {
    char name[IF_NAMESIZE];
    struct link_layer_address link_layer_address;
    int index;
    int flags;
};

struct umessage_interface_list {
    struct umessage base;
    size_t interface_count;
    struct umessage_interface_desc interface_list[];
};
#define UMESSAGE_INTERFACE_LIST_LENGTH(icount) (sizeof(struct umessage_interface_list) + (icount) * sizeof(struct umessage_interface_desc))
#define UMESSAGE_INTERFACE_LIST_COUNT(length)            \
    (((length) < sizeof(struct umessage_interface_list)) \
         ? 0                                             \
         : ((length) - sizeof(struct umessage_interface_list)) / sizeof(struct umessage_interface_desc))
#define UMESSAGE_INTERFACE_LIST_VALID(u, len)                                                       \
    (UMESSAGE_INTERFACE_MESSAGE_VALID(u, len) && (len) >= sizeof(struct umessage_interface_list) && \
     (u)->type == UMESSAGE_INTERFACE_LIST &&                                                        \
     ((struct umessage_interface_list*) (u))->interface_count == UMESSAGE_INTERFACE_LIST_COUNT(len))

enum umessage_input_request_type {
    UMESSAGE_INPUT_NUM_REQUESTS,
};

enum umessage_input_message_type {
    UMESSAGE_INPUT_KEY_EVENT,
    UMESSAGE_INPUT_MOUSE_EVENT,
    UMESSAGE_INPUT_NUM_EVENTS,
};
#define UMESSAGE_INPUT_MESSAGE_VALID(u, len) \
    (UMESSAGE_VALID(u, len) && (u)->category == UMESSAGE_INPUT && (u)->type < UMESSAGE_INPUT_NUM_EVENTS)

struct umessage_input_key_event {
    struct umessage base;
    struct key_event event;
};
#define UMESSAGE_INPUT_KEY_EVENT_VALID(u, len) \
    (UMESSAGE_INPUT_MESSAGE_VALID(u, len) && (u)->type == UMESSAGE_INPUT_KEY_EVENT && (len) >= sizeof(struct umessage_input_key_event))

struct umessage_input_mouse_event {
    struct umessage base;
    struct mouse_event event;
};
#define UMESSAGE_INPUT_MOUSE_EVENT_VALID(u, len) \
    (UMESSAGE_INPUT_MESSAGE_VALID(u, len) && (u)->type == UMESSAGE_INPUT_MOUSE_EVENT && (len) >= sizeof(struct umessage_input_mouse_event))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SYS_UMESSAGE_H */
