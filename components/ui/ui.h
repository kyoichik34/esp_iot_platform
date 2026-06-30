#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATUS_ONLINE = 0,
    STATUS_AWAY,
    STATUS_BUSY,
    STATUS_MAX
} status_t;

extern const char *ui_status_str[];

void ui_init(void);
void ui_update(const char* text);
void ui_update_network(void);
const char* ui_get_status(void);

#ifdef __cplusplus
}
#endif