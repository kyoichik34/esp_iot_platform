#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void presence_send_update_all_with(const char *status);
void presence_send_update_all(void);
void presence_poll_master(void);

#ifdef __cplusplus
}
#endif
