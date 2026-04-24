#include "hid.h"
#include <input/joycon.h>
#include <utils/btn.h>

static Input_t inputs = {0};
static u16 rbase_x = 0;
static u16 rbase_y = 0;

void hidInit()
{
	jc_init_hw();
}

Input_t *hidRead()
{
	jc_gamepad_rpt_t *controller = joycon_poll();

	inputs.buttons = 0;

	if (controller != NULL)
		inputs.buttons = controller->buttons;

	u8 btn = btn_read();
	inputs.volp = (btn & BTN_VOL_UP) ? 1 : 0;
	inputs.volm = (btn & BTN_VOL_DOWN) ? 1 : 0;
	inputs.power = (btn & BTN_POWER) ? 1 : 0;

	// Always allow hardware volume buttons to behave like up/down, regardless of
	// Joy-Con connection reporting.
	inputs.up = ((controller && controller->up) || inputs.volp) ? 1 : 0;
	inputs.down = ((controller && controller->down) || inputs.volm) ? 1 : 0;
	inputs.left = (controller && controller->left) ? 1 : 0;
	inputs.right = (controller && controller->right) ? 1 : 0;

	if (controller && controller->conn_r)
	{
		if ((rbase_x == 0 || rbase_y == 0) || controller->r3)
		{
			rbase_x = controller->rstick_x;
			rbase_y = controller->rstick_y;
		}

		inputs.rUp = (controller->rstick_y > rbase_y + 500) ? 1 : 0;
		inputs.rDown = (controller->rstick_y < rbase_y - 500) ? 1 : 0;
		inputs.rLeft = (controller->rstick_x < rbase_x - 500) ? 1 : 0;
		inputs.rRight = (controller->rstick_x > rbase_x + 500) ? 1 : 0;
	}
	else
	{
		inputs.rUp = 0;
		inputs.rDown = 0;
		inputs.rLeft = 0;
		inputs.rRight = 0;
	}

	inputs.a = ((controller && controller->a) || inputs.power) ? 1 : 0;

	return &inputs;
}

Input_t *hidWaitMask(u32 mask)
{
	Input_t *in = hidRead();

	while (in->buttons & mask)
		hidRead();

	while (!(in->buttons & mask))
		hidRead();

	return in;
}

Input_t *hidWait()
{
	Input_t *in = hidRead();

	while (in->buttons)
		hidRead();

	while (!(in->buttons))
		hidRead();

	return in;
}

bool hidConnected()
{
	jc_gamepad_rpt_t *controller = joycon_poll();
	return controller && controller->conn_l && controller->conn_r;
}
