#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);
void ui_update(const char* text);
void ui_update_network(void);
	
#ifdef __cplusplus
}
#endif