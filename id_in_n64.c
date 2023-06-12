/*
Omnispeak: A Commander Keen Reimplementation
Copyright (C) 2018 Omnispeak Authors
Copyright (C) 2021 Ryan Wendland (N64 - Libdragon port)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <libdragon.h>

#include "id_in.h"

static void IN_N64_PumpEvents()
{
    controller_scan();

    //There's a few joystick buttons that need to be injected as keypressed
    struct controller_data keys;
    keys = get_keys_down();

    if(keys.c[0].err) return;

    if (keys.c[0].start)   IN_HandleKeyDown(IN_SC_Escape, 0);
    //Any of these will trigger to status menu
    if (keys.c[0].L)       IN_HandleKeyDown(IN_SC_Enter, 0);
    if (keys.c[0].C_up)    IN_HandleKeyDown(IN_SC_Enter, 0);
    if (keys.c[0].C_down)  IN_HandleKeyDown(IN_SC_Enter, 0);
    if (keys.c[0].C_left)  IN_HandleKeyDown(IN_SC_Enter, 0);
    if (keys.c[0].C_right) IN_HandleKeyDown(IN_SC_Enter, 0);

    keys = get_keys_up();
    if (keys.c[0].start)   IN_HandleKeyUp(IN_SC_Escape, 0);
    if (keys.c[0].L)       IN_HandleKeyUp(IN_SC_Enter, 0);
    if (keys.c[0].C_up)    IN_HandleKeyUp(IN_SC_Enter, 0);
    if (keys.c[0].C_down)  IN_HandleKeyUp(IN_SC_Enter, 0);
    if (keys.c[0].C_left)  IN_HandleKeyUp(IN_SC_Enter, 0);
    if (keys.c[0].C_right) IN_HandleKeyUp(IN_SC_Enter, 0);
}

static void IN_N64_WaitKey()
{
    return;
}

static void IN_N64_Startup(bool disableJoysticks)
{
    controller_init();
    IN_SetControlType(0, IN_ctrl_Joystick1);
    IN_SetJoyConf(IN_joy_jump, 0);
    IN_SetJoyConf(IN_joy_pogo, 1);
    IN_SetJoyConf(IN_joy_fire, 2);
    IN_SetJoyConf(IN_joy_deadzone, 60);
}

static bool IN_N64_StartJoy(int joystick)
{
    return true;
}

static void IN_N64_StopJoy(int joystick)
{
    return;
}

static bool IN_N64_JoyPresent(int joystick)
{
    int controllers = get_controllers_present();
    int mask = (joystick == 0) ? CONTROLLER_1_INSERTED :
               (joystick == 1) ? CONTROLLER_2_INSERTED :
               (joystick == 2) ? CONTROLLER_3_INSERTED :
               (joystick == 3) ? CONTROLLER_4_INSERTED : 0;

    return ((controllers & mask) > 0);
}

static void IN_N64_JoyGetAbs(int joystick, int *x, int *y)
{
    struct controller_data keys = get_keys_pressed();
    int x_val = keys.c[joystick].x * 256;
    int y_val = -keys.c[joystick].y * 256 - 1;

    //Map d-pad to analog stick too
    switch (get_dpad_direction(joystick))
    {
        case 0:
            x_val = 32767;
            break;
        case 1:
            x_val = 32767;
            y_val = -32768;
            break;
        case 2:
            y_val = -32768;
            break;
        case 3:
            x_val = -32768;
            y_val = -32768;
            break;
        case 4:
            x_val = -32768;
            break;
        case 5:
            x_val = -32768;
            y_val = 32767;
            break;
        case 6:
            y_val = 32767;
            break;
        case 7:
            y_val = 32767;
            x_val = 32767;
            break;
    }

    if (x != NULL)
    {
        *x = x_val;
    }
    if (y != NULL)
    {
        *y = y_val;
    }
}

static uint16_t IN_N64_JoyGetButtons(int joystick)
{
    uint16_t mask = 0;
    struct controller_data keys = get_keys_pressed();
    if (IN_N64_JoyPresent(joystick) == false || keys.c[joystick].err)
    {
        return mask;
    }

    if (keys.c[joystick].A)       mask |= (1 << IN_joy_jump);
    if (keys.c[joystick].B)       mask |= (1 << IN_joy_pogo);
    if (keys.c[joystick].start)   mask |= (1 << IN_joy_menu);
    if (keys.c[joystick].Z)       mask |= (1 << IN_joy_fire);
    if (keys.c[joystick].R)       mask |= (1 << IN_joy_fire);
    if (keys.c[joystick].L)       mask |= (1 << IN_joy_status);
    if (keys.c[joystick].C_up)    mask |= (1 << IN_joy_status);
    if (keys.c[joystick].C_down)  mask |= (1 << IN_joy_status);
    if (keys.c[joystick].C_left)  mask |= (1 << IN_joy_status);
    if (keys.c[joystick].C_right) mask |= (1 << IN_joy_status);

    return mask;
}

static const char *IN_N64_JoyGetName(int joystick)
{
    return "N64 Joystick";
}

static const char *IN_N64_GetButtonName(int joystick, int index)
{
    return "No Name";
}

static void IN_N64_StartTextInput(const char *reason, const char *oldText)
{

}

static void IN_N64_StopTextInput()
{

}

static IN_Backend in_n64_backend = {
    .startup = IN_N64_Startup,
    .shutdown = 0,
    .pumpEvents = IN_N64_PumpEvents,
    .waitKey = IN_N64_WaitKey,
    .joyStart = IN_N64_StartJoy,
    .joyStop = IN_N64_StopJoy,
    .joyPresent = IN_N64_JoyPresent,
    .joyGetAbs = IN_N64_JoyGetAbs,
    .joyGetButtons = IN_N64_JoyGetButtons,
    .joyGetName = IN_N64_JoyGetName,
    .joyGetButtonName = IN_N64_GetButtonName,
    .startTextInput = IN_N64_StartTextInput,
    .stopTextInput = IN_N64_StopTextInput,
    .joyAxisMin = -32767,
    .joyAxisMax = 32768,
    .supportsTextEvents = false
};

IN_Backend *IN_Impl_GetBackend()
{
    return &in_n64_backend;
}
