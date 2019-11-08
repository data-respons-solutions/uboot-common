#ifndef __DR_BOOTSPLASH_H__
#define __DR_BOOTSPLASH_H__

/**
 * bootsplash_load() - load bootsplash from spi flash
 *
 * Bootsplash image addr set to env "splashimage"
 *
 * @return 0 if ok, -errno on error
 */
int bootsplash_load(void);

#endif // __DR_BOOTSPLASH_H__
