#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATUS_READY  = 0,
    STATUS_ONLINE = 1,
    STATUS_AWAY   = 2,
    STATUS_BUSY   = 3,
    STATUS_MAX
} status_t;

#define PRESENCE_COMMENT_MAX 128

void presence_send_update_all_with();
void presence_send_update_all(void);
void presence_poll_master(void);

typedef struct {
    status_t status;
    char comment[PRESENCE_COMMENT_MAX];
} presence_state_t;

extern presence_state_t g_presence;

#ifdef __cplusplus
}
#endif
