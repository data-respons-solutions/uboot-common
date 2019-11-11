#ifndef __DR_BOOTCOUNT_H__
#define __DR_BOOTCOUNT_H__

/**
 * nvram_bootcount_store() - store bootcounter value
 *
 * @return 0 if ok, -errno on error
 */
int nvram_bootcount_store(uint32_t val);

/**
 * nvram_bootcount_load() - load bootcounter value
 *
 * @return bootcounter value
 */
uint32_t nvram_bootcount_load(void);

#endif // __DR_BOOTCOUNT_H__
