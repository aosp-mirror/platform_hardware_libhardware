/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <hardware/led.h>

typedef int  (*LedFunc)(unsigned, int, int);

#ifdef CONFIG_LED_SARDINE
extern int  sardine_set_led_state(unsigned, int, int);
#  define HW_LED_FUNC  sardine_set_led_state
#endif

#ifdef CONFIG_LED_TROUT
extern int  trout_set_led_state(unsigned, int, int);
#  define HW_LED_FUNC  trout_set_led_state
#endif

#ifdef CONFIG_LED_QEMU
#include "qemu.h"
static int
qemu_set_led_state(unsigned color, int on, int off)
{
    qemu_control_command( "set_led_state:%08x:%d:%d", color, on, off );
    return 0;
}
#endif

int
set_led_state(unsigned color, int on, int off)
{
#ifdef CONFIG_LED_QEMU
    QEMU_FALLBACK(set_led_state(color,on,off));
#endif
#ifdef HW_LED_FUNC
    return HW_LED_FUNC(color, on, off);
#else
    return 0;
#endif
}
