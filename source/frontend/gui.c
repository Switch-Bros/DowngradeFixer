/*
 * Copyright (c) 2018-2021 CTCaer
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

#include "../gfx/gfx.h"
#include <mem/heap.h>
#include <rtc/max77620-rtc.h>
#include <storage/nx_sd.h>
#include <utils/util.h>
#include <utils/sprintf.h>

#include <string.h>

int save_fb_to_bmp()
{
	static u32 timer = 0;
	if (get_tmr_ms() < timer)
		return 1;

	const u32 bmp_width = 1280;
	const u32 bmp_height = 720;
	const u32 pixel_data_size = bmp_width * bmp_height * 4;
	const u32 file_size = pixel_data_size + 0x36;
	u8 *bitmap = malloc(file_size);
	u32 *fb = malloc(pixel_data_size);
	u32 *fb_ptr = gfx_ctxt.fb;
	if (!bitmap || !fb)
	{
		free(bitmap);
		free(fb);
		return 1;
	}

	for (int y = bmp_height - 1; y >= 0; y--)
	{
		for (u32 x = 0; x < bmp_width; x++)
			fb[(bmp_height - 1 - y) * bmp_width + x] = fb_ptr[y + (1279 - x) * 720];
	}

	memcpy(bitmap + 0x36, fb, pixel_data_size);

	typedef struct _bmp_t
	{
		u16 magic;
		u32 size;
		u32 rsvd;
		u32 data_off;
		u32 hdr_size;
		u32 width;
		u32 height;
		u16 planes;
		u16 pxl_bits;
		u32 comp;
		u32 img_size;
		u32 res_h;
		u32 res_v;
		u64 rsvd2;
	} __attribute__((packed)) bmp_t;

	bmp_t *bmp = (bmp_t *)bitmap;

	bmp->magic    = 0x4D42;
	bmp->size     = file_size;
	bmp->rsvd     = 0;
	bmp->data_off = 0x36;
	bmp->hdr_size = 40;
	bmp->width    = bmp_width;
	bmp->height   = bmp_height;
	bmp->planes   = 1;
	bmp->pxl_bits = 32;
	bmp->comp     = 0;
	bmp->img_size = pixel_data_size;
	bmp->res_h    = 2834;
	bmp->res_v    = 2834;
	bmp->rsvd2    = 0;

	sd_mount();

	f_mkdir("sd:/switch");
	f_mkdir("sd:/switch/screenshot");

	rtc_time_t time;
	max77620_rtc_get_time(&time);

	char path[0x80];
	s_printf(path, "sd:/switch/screenshot/downgradefixer_%04d%02d%02d_%02d%02d%02d.bmp",
		time.year, time.month, time.day, time.hour, time.min, time.sec);

	int res = sd_save_to_file(bitmap, file_size, path);

	free(bitmap);
	free(fb);

	timer = get_tmr_ms() + 1000;

	return res;
}
