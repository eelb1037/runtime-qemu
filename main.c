// Copyright 2014 Technical Machine, Inc. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
// #include <luajit.h>

#include <stdlib.h>
#include <string.h>

#include <tm.h>
#include <colony.h>

/** \brief  Enable IRQ Interrupts

  This function enables IRQ interrupts by clearing the I-bit in the CPSR.
  Can only be executed in Privileged modes.
 */
__attribute__( ( always_inline ) ) static inline void __enable_irq(void)
{
  __asm__ volatile ("cpsie i" : : : "memory");
}


/** \brief  Disable IRQ Interrupts

  This function disables IRQ interrupts by setting the I-bit in the CPSR.
  Can only be executed in Privileged modes.
 */
__attribute__( ( always_inline ) ) static inline void __disable_irq(void)
{
  __asm__ volatile ("cpsid i" : : : "memory");
}

/** \brief  Wait For Interrupt

    Wait For Interrupt is a hint instruction that suspends execution
    until one of a number of events occurs.
 */
__attribute__( ( always_inline ) ) static inline void __WFI(void)
{
  __asm__ volatile ("wfi");
}


void hw_timer_update_interrupt()
{
  tm_event_trigger(&tm_timer_event);
}

void tm_events_lock() { __disable_irq(); }
void tm_events_unlock() { __enable_irq(); }

void hw_wait_for_event() {
    __disable_irq();
    if (!tm_events_pending()) {
        // Check for events after disabling interrupts to avoid the race
        // condition where an event comes from an interrupt between check and
        // sleep. Processor still wakes from sleep on interrupt request even
        // when interrupts are disabled.
        __WFI();
    }
    __enable_irq();
}

int tm_entropy_seed() { return 0; }

double tm_timestamp () { return 0; }
int tm_timestamp_update (double millis) { (void) millis; return 0; }

void tm_uptime_init () { }
uint32_t tm_uptime_micro () { return 0; }

void tm_log(char level, const char* string, unsigned length) {
    (void) level;
    while (length-- > 0) {
        putchar(string[0]);
        string++;
    }
    putchar('\n');
}

/*

hw_timer_update_interrupt
tm_log

*/



unsigned char test_lua[] = {
    #include "test.h"
    , 0x00
};

/**
 * Run
 */

tm_fs_ent* tm_fs_root;

int traceback (lua_State *L)
{
  lua_getfield(L, LUA_GLOBALSINDEX, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }
  // lua_pushinteger(L, 2);   skip this function and traceback 
  lua_call(L, 0, 1);  /* call debug.traceback */
  return 1;
}

void lua_interrupt_hook(lua_State* L, lua_Debug *ar)
{
    (void) ar;
  traceback(L);
  printf("SIGINT %s\n", lua_tostring(L, -1));
  exit(0);
}

void hw_wait_for_event();

int main ()
{
  int ret = 0;

  setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
  setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

  printf("\n\n--> START.\n");

  tm_fs_root = tm_fs_dir_create_entry();
  tm_fs_file_handle ok;
  ret = tm_fs_open(&ok, tm_fs_root, "test.js", TM_CREAT);
  // printf("open(): %s\n", strerror(-ret));
  ret = tm_fs_write (&ok, test_lua, strlen((const char *) test_lua));
  // printf("write(): %s\n", strerror(-ret));
  ret = tm_fs_close(&ok);
  // printf("close(): %s\n", strerror(-ret));

  colony_runtime_open();
  // tm_eval_lua()
  const char *argv[3] = {"colony", "test.js", NULL};

  ret = tm_runtime_run(argv[1], argv, 2);
  colony_runtime_close();

  printf("--> END.\n");

  return ret;
}