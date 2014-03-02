/* drivers/i2c/chips/huawei_qwerty.h
 *
 * Copyright (C) 2007-2009 Huawei.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
 
#ifndef __HUAWEI_QWERTY_H
#define __HUAWEI_QWERTY_H


#define ADP5587_KEYPAD_ROW_SIZE	5
#define	ADP5587_KEYPAD_COL_SIZE	10

#define KEYMAP_INDEX(row, col) ((row)*ADP5587_KEYPAD_COL_SIZE + (col))

#define KEYMAP_SIZE ADP5587_KEYPAD_ROW_SIZE * ADP5587_KEYPAD_COL_SIZE

static unsigned short keypad_keymap_7x27a[KEYMAP_SIZE] = {
	[KEYMAP_INDEX(0, 0)] = KEY_Q,
	[KEYMAP_INDEX(0, 1)] = KEY_W,
	[KEYMAP_INDEX(0, 2)] = KEY_E,
	[KEYMAP_INDEX(0, 3)] = KEY_R,
	[KEYMAP_INDEX(0, 4)] = KEY_T,
	[KEYMAP_INDEX(0, 5)] = KEY_Y,
	[KEYMAP_INDEX(0, 6)] = KEY_U,
	[KEYMAP_INDEX(0, 7)] = KEY_I,
	[KEYMAP_INDEX(0, 8)] = KEY_O,
	[KEYMAP_INDEX(0, 9)] = KEY_P,

	[KEYMAP_INDEX(1, 0)] = KEY_A,
	[KEYMAP_INDEX(1, 1)] = KEY_S,
	[KEYMAP_INDEX(1, 2)] = KEY_D,
	[KEYMAP_INDEX(1, 3)] = KEY_F,
	[KEYMAP_INDEX(1, 4)] = KEY_G,
	[KEYMAP_INDEX(1, 5)] = KEY_H,
	[KEYMAP_INDEX(1, 6)] = KEY_J,
	[KEYMAP_INDEX(1, 7)] = KEY_K,
	[KEYMAP_INDEX(1, 8)] = KEY_L,
	[KEYMAP_INDEX(1, 9)] = KEY_BACKSPACE,

	[KEYMAP_INDEX(2, 1)] = KEY_Z,
	[KEYMAP_INDEX(2, 2)] = KEY_X,
	[KEYMAP_INDEX(2, 3)] = KEY_C,
	[KEYMAP_INDEX(2, 4)] = KEY_V,
	[KEYMAP_INDEX(2, 5)] = KEY_B,
	[KEYMAP_INDEX(2, 6)] = KEY_N,
	[KEYMAP_INDEX(2, 7)] = KEY_M,
	[KEYMAP_INDEX(2, 8)] = KEY_DOT,		//. 
	[KEYMAP_INDEX(2, 9)] = KEY_ENTER,	//enter

	[KEYMAP_INDEX(3, 0)] = KEY_LEFTALT, 
	[KEYMAP_INDEX(3, 2)] = KEY_LEFTSHIFT, //aA
	[KEYMAP_INDEX(3, 3)] = KEY_0,		//smaiey
	[KEYMAP_INDEX(3, 5)] = KEY_SPACE,
	[KEYMAP_INDEX(3, 6)] = KEY_F24,		//symbol 
	[KEYMAP_INDEX(3, 7)] = KEY_PLAYCD,		


	[KEYMAP_INDEX(4, 1)] = KEY_VOLUMEDOWN,	
	[KEYMAP_INDEX(4, 4)] = KEY_VOLUMEUP,
};
#endif

