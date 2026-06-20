#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void wifi_init_sta(void);

/* ===== ó‘Ô‹¤—L ===== */
extern char g_ipv4_str[32];
extern char g_ipv6_str[64];
extern int  g_ipv4_ready;
extern int  g_ipv6_ready;
extern int  g_rssi;


#ifdef __cplusplus
}
#endif