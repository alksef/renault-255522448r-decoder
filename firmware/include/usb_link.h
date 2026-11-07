#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void usb_link_init(void);
void usb_link_poll(void);
bool usb_link_send(const char *line);
bool usb_link_configured(void);
uint32_t usb_link_dropped(void);
void app_receive(char byte);
void app_usb_reset(void);
#ifdef __cplusplus
}
#endif
