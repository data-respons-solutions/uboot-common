
#include <common.h>
#include <i2c.h>
#include <power/pfuze100_pmic.h>
#include <linux/errno.h>
#include "pfuze100.h"

struct {
	pf100_regs	reg;
	char		*desc;
} reg_table[] = {
		{SW1AB, "SW1AB"},
		{SW1C, "SW1C"},
		{SW3AB, "SW3AB"},
		{VGEN4, "VGEN4"},
		{0, NULL}
};

char *pf100_reg_str(pf100_regs reg)
{
	for (int i = 0; reg_table[i].desc; ++i) {
		if (reg_table[i].reg == reg) {
			return reg_table[i].desc;
		}
	}

	return NULL;
}

int pfuze100_setup(int i2c_bus, int addr)
{
	int ret = 0;

	i2c_set_bus_num(i2c_bus);
	ret = i2c_probe(addr);
	if (ret)
	{
		return -ENODEV;
	}

	return 0;
}

int pfuze100_set(int i2c_bus, int addr, pf100_regs reg, int mV)
{
	u8 values[2] = {0, 0};

	i2c_set_bus_num(i2c_bus);
	switch (reg) {
	case SW1AB:
		if ((mV < 300) || (mV > 1875)) {
			return -EINVAL;
		}
		values[0] = (mV - 300) / 25;
		if(i2c_write(addr, PFUZE100_SW1ABVOL, 1, values, 1)) {
			return -EIO;
		}
		break;

	case SW1C:
		if ((mV < 300) || (mV > 1875)) {
			return -EINVAL;
		}
		values[0] = (mV - 300) / 25;
		if (i2c_write(addr, PFUZE100_SW1CVOL, 1, values, 1)) {
			return -EIO;
		}
		break;

	case SW3AB:
		if ((mV < 400) || (mV > 3300)) {
			return -EINVAL;
		}
		values[0] = (mV - 400) / 25;
		if (i2c_write(addr, PFUZE100_SW3AVOL, 1, values, 1)) {
			return -EIO;
		}
		if (i2c_write(addr, PFUZE100_SW3BVOL, 1, values, 1)) {
			return -EIO;
		}
		break;

	case VGEN4:
		if ((mV < 1800) || (mV > 3300)) {
			return -EINVAL;
		}
		values[0] = ((mV - 1800) * 15) / 1500;
		if (i2c_write(addr, PFUZE100_VGEN4VOL, 1, values, 1)) {
			return -EIO;
		}
		break;

	default:
		return -ENODEV;
		break;
	}

	return 0;
}
