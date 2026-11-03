#ifndef BYS_TEST_H
#define BYS_TEST_H

#include "bys_uart.h"

#ifdef BYS_TEST_MODE
void bys_test_init(void);
void bys_test_poll_next(void);
void bys_test_tick(void);
uint8 bys_test_handle_app_cmd(uint8 *buf, uint8 len);
#endif

#endif /* BYS_TEST_H */
