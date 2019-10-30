
#ifndef __DR_PFUZE100__H__
#define __DR_PFUZE100__H__


typedef enum { SW1AB = 1, SW1C, SW3AB, VGEN4 } pf100_regs;

char *pf100_reg_str(pf100_regs reg);

int pfuze100_setup(int i2c_bus, int addr);
int pfuze100_set(int i2c_bus, int addr, pf100_regs reg, int mV);


#endif // __DR_PFUZE100__H__
