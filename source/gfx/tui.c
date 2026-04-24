/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <display/di.h>
#include "tui.h"
#include "../frontend/gui.h"
#include "../hid/hid.h"
#include "../config.h"
#include <power/max17050.h>
#include <utils/util.h>
#include <utils/sprintf.h>

extern hekate_config h_cfg;

static const char *tui_bottom_legend = "D-pad/Vol : Move, A/PWR:Select   Hold+: Screenshot";

static void tui_draw_layout(void)
{
	char title[64];
	s_printf(title, "DowngradeFixer %d.%d.%d", LP_VER_MJ, LP_VER_MN, LP_VER_BF);

	gfx_boxGrey(0, 16, 1279, 703, 0x1B);
	gfx_draw_title_bar(title);
	gfx_draw_bottom_bar(tui_bottom_legend);
}

static void tui_draw_menu_entry(menu_t *menu, int entry_idx, bool selected)
{
	int y = UI_MENU_START_Y + (entry_idx * UI_MENU_SPACING);

	gfx_boxGrey(0, y, 1279, y + UI_MENU_SPACING - 1, 0x1B);
	gfx_con_setpos(UI_MENU_START_X, y);

	if (selected)
		gfx_con_setcol(0xFF1B1B1B, 1, COLOR_SOFT_WHITE);
	else
		gfx_con_setcol(COLOR_SOFT_WHITE, 1, 0xFF1B1B1B);

	if (menu->ents[entry_idx].type != MENT_CHGLINE)
	{
		if (selected)
			gfx_printf(" %s", menu->ents[entry_idx].caption);
		else
			gfx_printf("%k %s", menu->ents[entry_idx].color, menu->ents[entry_idx].caption);
	}
	if (menu->ents[entry_idx].type == MENT_MENU)
		gfx_printf("%k...", 0xFF0099EE);
}

static void tui_draw_notice(const char *msg, u32 color)
{
	u32 cx, cy;

	if (!msg)
		return;

	gfx_con_getpos(&cx, &cy);
	gfx_con_setcol(color, 1, 0xFF1B1B1B);
	gfx_con_setpos(UI_NOTIFY_X, UI_NOTIFY_Y);
	gfx_printf("%s                              ", msg);
	gfx_con_setpos(cx, cy);
}

static void tui_clear_notice(void)
{
	u32 cx, cy;

	gfx_con_getpos(&cx, &cy);
	gfx_con_setcol(COLOR_SOFT_WHITE, 1, 0xFF1B1B1B);
	gfx_con_setpos(UI_NOTIFY_X, UI_NOTIFY_Y);
	gfx_printf("                                                  ");
	gfx_con_setpos(cx, cy);
}

static void tui_take_screenshot(void)
{
	int res = save_fb_to_bmp();
	if (!res)
		tui_draw_notice("Screenshot saved!", COLOR_CYAN_L);
	else
		tui_draw_notice("Screenshot failed!", COLOR_ERROR);

	msleep(1000);
	tui_clear_notice();

	if (h_cfg.emummc_force_disable)
		tui_draw_notice("No emuMMC config found.", COLOR_RED_D);
}

static int tui_next_selectable(menu_t *menu, int idx, int cnt, bool forward)
{
	do
	{
		if (forward)
			idx = (idx < (cnt - 1)) ? idx + 1 : 0;
		else
			idx = (idx > 0) ? idx - 1 : cnt - 1;
	} while (menu->ents[idx].type == MENT_CAPTION ||
		menu->ents[idx].type == MENT_CHGLINE);

	return idx;
}

static u32 tui_input_state(const Input_t *input)
{
	u32 state = 0;

	if (input->a)
		state |= BIT(0);
	if (input->down)
		state |= BIT(1);
	if (input->up)
		state |= BIT(2);
	if (input->left)
		state |= BIT(3);
	if (input->right)
		state |= BIT(4);
	if (input->rDown)
		state |= BIT(5);
	if (input->rUp)
		state |= BIT(6);
	if (input->volp)
		state |= BIT(7);
	if (input->volm)
		state |= BIT(8);
	if (input->power)
		state |= BIT(9);

	return state;
}

void tui_sbar(bool force_update)
{
	u32 cx, cy;
	static u32 sbar_time_keeping = 0;

	u32 timePassed = get_tmr_s() - sbar_time_keeping;
	if (!force_update)
		if (timePassed < 5)
			return;

	u8 prevFontSize = gfx_con.fntsz;
	gfx_con.fntsz = 16;
	sbar_time_keeping = get_tmr_s();

	u32 battPercent = 0;
	gfx_con_getpos(&cx, &cy);
	gfx_con_setpos(1050, 704);

	max17050_get_property(MAX17050_RepSOC, (int *)&battPercent);

	u8 saved_fillbg = gfx_con.fillbg;
	u32 saved_bgcol = gfx_con.bgcol;

	gfx_printf("%K%k Batt: %d%%", 0xFF3D3D3D, 0xFF00D8FF,
		battPercent >> 8);

	gfx_con.fillbg = saved_fillbg;
	gfx_con.bgcol = saved_bgcol;

	gfx_con.fntsz = prevFontSize;
	gfx_con_setpos(cx, cy);
}

void tui_pbar(int x, int y, u32 val, u32 fgcol, u32 bgcol)
{
	u32 cx, cy;
	if (val > 200)
		val = 200;

	gfx_con_getpos(&cx, &cy);

	gfx_con_setpos(x, y);

	gfx_printf("%k[%3d%%]%k", fgcol, val, 0xFFCCCCCC);

	x += 7 * gfx_con.fntsz;

	for (u32 i = 0; i < (gfx_con.fntsz >> 3) * 6; i++)
	{
		gfx_line(x, y + i + 1, x + 3 * val, y + i + 1, fgcol);
		gfx_line(x + 3 * val, y + i + 1, x + 3 * 100, y + i + 1, bgcol);
	}

	gfx_con_setpos(cx, cy);

	tui_sbar(false);
}

void *tui_do_menu(menu_t *menu)
{
	int idx = 0, prev_idx = -1, cnt = 0x7FFFFFFF;
	int need_full_redraw = 1;
	u32 btn_last = tui_input_state(hidRead());
	u32 vol_press_start = 0;

	while (true)
	{
		while (menu->ents[idx].type == MENT_CAPTION ||
			menu->ents[idx].type == MENT_CHGLINE)
		{
			if (prev_idx <= idx || (!idx && prev_idx == cnt - 1))
			{
				idx++;
				if (idx > (cnt - 1))
				{
					idx = 0;
					prev_idx = 0;
				}
			}
			else
			{
				idx--;
				if (idx < 0)
				{
					idx = cnt - 1;
					prev_idx = cnt;
				}
			}
		}
		prev_idx = idx;

		if (need_full_redraw)
		{
			tui_draw_layout();

			for (cnt = 0; menu->ents[cnt].type != MENT_END; cnt++)
				tui_draw_menu_entry(menu, cnt, cnt == idx);

			if (h_cfg.emummc_force_disable)
				tui_draw_notice("No emuMMC config found.", COLOR_RED_D);

			tui_sbar(true);
			need_full_redraw = 0;
		}

		display_backlight_brightness(h_cfg.backlight, 1000);

		Input_t *input = hidRead();
		u32 btn = tui_input_state(input);

		if (btn == btn_last)
		{
			if (input->volp && !input->volm && !input->power)
			{
				if (vol_press_start == 0)
					vol_press_start = get_tmr_ms();
				else if (get_tmr_ms() - vol_press_start >= 1000)
				{
					vol_press_start = 0;
					btn_last = 0;
					tui_take_screenshot();
					while (tui_input_state(hidRead()))
						msleep(10);
				}
			}
			else
			{
				vol_press_start = 0;
			}

			msleep(10);
			tui_sbar(false);
			continue;
		}

		btn_last = btn;

		if (!btn)
		{
			vol_press_start = 0;
			msleep(10);
			tui_sbar(false);
			continue;
		}

		if (input->a)
		{
			ment_t *ent = &menu->ents[idx];
			switch (ent->type)
			{
			case MENT_HANDLER:
				ent->handler(ent->data);
				break;
			case MENT_MENU:
				return tui_do_menu(ent->menu);
			case MENT_DATA:
				return ent->data;
			case MENT_BACK:
				return NULL;
			case MENT_HDLR_RE:
				ent->handler(ent);
				if (!ent->data)
					return NULL;
				break;
			default:
				break;
			}
			gfx_con.fntsz = 16;
			need_full_redraw = 1;
		}
		else if (input->down || input->rDown || input->right)
		{
			int old_idx = idx;
			idx = tui_next_selectable(menu, idx, cnt, true);
			prev_idx = old_idx;
			tui_draw_menu_entry(menu, old_idx, false);
			tui_draw_menu_entry(menu, idx, true);
		}
		else if (input->up || input->rUp || input->left)
		{
			int old_idx = idx;
			idx = tui_next_selectable(menu, idx, cnt, false);
			prev_idx = old_idx;
			tui_draw_menu_entry(menu, old_idx, false);
			tui_draw_menu_entry(menu, idx, true);
		}

		tui_sbar(false);
	}

	return NULL;
}
