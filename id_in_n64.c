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
    joypad_buttons_t keys;
    joypad_poll();

    //There's a few joystick buttons that need to be injected as keypresses
    keys = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    if (keys.start)   IN_HandleKeyDown(IN_SC_Escape, 0);
    if (keys.l)       IN_HandleKeyDown(IN_SC_Enter, 0);
    if (keys.c_up)    IN_HandleKeyDown(IN_SC_Enter, 0);
    if (keys.c_down)  IN_HandleKeyDown(IN_SC_Enter, 0);
    if (keys.c_left)  IN_HandleKeyDown(IN_SC_Enter, 0);
    if (keys.c_right) IN_HandleKeyDown(IN_SC_Enter, 0);

    keys = joypad_get_buttons_released(JOYPAD_PORT_1);
    if (keys.start)   IN_HandleKeyUp(IN_SC_Escape, 0);
    if (keys.l)       IN_HandleKeyUp(IN_SC_Enter, 0);
    if (keys.c_up)    IN_HandleKeyUp(IN_SC_Enter, 0);
    if (keys.c_down)  IN_HandleKeyUp(IN_SC_Enter, 0);
    if (keys.c_left)  IN_HandleKeyUp(IN_SC_Enter, 0);
    if (keys.c_right) IN_HandleKeyUp(IN_SC_Enter, 0);
}

static void IN_N64_WaitKey()
{
    return;
}

static void IN_N64_Startup(bool disableJoysticks)
{
    joypad_init();
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
    return joypad_is_connected(joystick);
}

static void IN_N64_JoyGetAbs(int joystick, int *x, int *y)
{
    int x_val = 0, y_val = 0;

    //Map d-pad to analog stick too
    switch (joypad_get_direction(joystick, JOYPAD_2D_LH))
    {
        case JOYPAD_8WAY_RIGHT:
            x_val = 32767;
            break;
        case JOYPAD_8WAY_UP_RIGHT:
            x_val = 32767;
            y_val = -32768;
            break;
        case JOYPAD_8WAY_UP:
            y_val = -32768;
            break;
        case JOYPAD_8WAY_UP_LEFT:
            x_val = -32768;
            y_val = -32768;
            break;
        case JOYPAD_8WAY_LEFT:
            x_val = -32768;
            break;
        case JOYPAD_8WAY_DOWN_LEFT:
            x_val = -32768;
            y_val = 32767;
            break;
        case JOYPAD_8WAY_DOWN:
            y_val = 32767;
            break;
        case JOYPAD_8WAY_DOWN_RIGHT:
            y_val = 32767;
            x_val = 32767;
            break;
        case JOYPAD_8WAY_NONE:
        default:
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
    joypad_inputs_t inputs = joypad_get_inputs(joystick);

    if (IN_N64_JoyPresent(joystick) == false)
    {
        return mask;
    }

    if (inputs.btn.a)       mask |= (1 << IN_joy_jump);
    if (inputs.btn.b)       mask |= (1 << IN_joy_pogo);
    if (inputs.btn.start)   mask |= (1 << IN_joy_menu);
    if (inputs.btn.z)       mask |= (1 << IN_joy_fire);
    if (inputs.btn.r)       mask |= (1 << IN_joy_fire);
    if (inputs.btn.l)       mask |= (1 << IN_joy_status);
    if (inputs.btn.c_up)    mask |= (1 << IN_joy_status);
    if (inputs.btn.c_down)  mask |= (1 << IN_joy_status);
    if (inputs.btn.c_left)  mask |= (1 << IN_joy_status);
    if (inputs.btn.c_right) mask |= (1 << IN_joy_status);

    return mask;
}

static const char *IN_N64_JoyGetName(int joystick)
{
    return "N64 Joystick";
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
    .joyAxisMin = -32767,
    .joyAxisMax = 32768,
};

IN_Backend *IN_Impl_GetBackend()
{
    return &in_n64_backend;
}
