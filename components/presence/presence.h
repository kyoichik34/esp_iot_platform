#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PRESENCE_STATUS_MAX 16
#define PRESENCE_COMMENT_MAX 128

void presence_send_update_all_with();
void presence_send_update_all(void);
void presence_poll_master(void);

typedef struct {
    char status[PRESENCE_STATUS_MAX];
    char comment[PRESENCE_COMMENT_MAX];
} presence_state_t;

extern presence_state_t g_presence;

#ifdef __cplusplus
}
#endif
