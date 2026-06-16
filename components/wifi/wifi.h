#pragma once

void wifi_init_sta(void);

/* ===== ó‘Ô‹¤—L ===== */

#ifdef __cplusplus
extern "C" {
#endif

extern char g_ipv4_str[32];
extern char g_ipv6_str[64];
extern int  g_rssi;

#ifdef __cplusplus
}
#endif