/*
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/types.h>
#include <openbmc/kv.h>
#include <openbmc/pal.h>
#include <openbmc/libgpio.h>

// Thread to handle LED state of the SLED
static void* led_sync_handler()
{
  int ret;
  char identify[MAX_VALUE_LEN] = {0};
  gpio_desc_t *desc = gpio_open_by_shadow("PWR_ID_LED_N");
  gpio_value_t value;

  if (!desc) {
    syslog(LOG_ERR, "ID LED open failed\n");
    return NULL;
  }

  while (1) {
    sleep(1);

    // Handle IDENTIFY LED
    memset(identify, 0x0, sizeof(identify));
    ret = pal_get_key_value("identify_sled", identify);
    if (ret == 0) {
      value = strcmp(identify, "on") == 0? GPIO_VALUE_LOW: GPIO_VALUE_HIGH;
      if (gpio_set_value(desc, value)) {
        syslog(LOG_ERR, "ID LED control failed\n");
      }
    }
  }

  return NULL;
}

static void* pwr_btn_handler()
{
  // TODO:
  //   Check if we need to block power button

  return NULL;
}

int main (int argc, char * const argv[])
{
  pthread_t tid_sync_led;
  pthread_t tid_pwr_btn;
  int rc;
  int pid_file;

  pid_file = open("/var/run/front-paneld.pid", O_CREAT | O_RDWR, 0666);
  rc = flock(pid_file, LOCK_EX | LOCK_NB);
  if(rc) {
    if(EWOULDBLOCK == errno) {
      syslog(LOG_ERR, "Another front-paneld instance is running...\n");
      exit(-1);
    }
  } else {
    openlog("front-paneld", LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "front-paneld: daemon started");
  }

  if (pthread_create(&tid_sync_led, NULL, led_sync_handler, NULL) < 0) {
    syslog(LOG_WARNING, "pthread_create for led sync error\n");
    exit(1);
  }

  if (pthread_create(&tid_pwr_btn, NULL, pwr_btn_handler, NULL) < 0) {
    syslog(LOG_WARNING, "pthread_create for reset button error\n");
    exit(1);
  }

  pthread_join(tid_sync_led, NULL);
  pthread_join(tid_pwr_btn, NULL);
  return 0;
}
