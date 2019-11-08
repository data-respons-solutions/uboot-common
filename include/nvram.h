#ifndef __DR_NVRAM_H__
#define __DR_NVRAM_H__

/**
 * nvram_get() - Look up the value of an nvram variable
 *
 * @varname:	Variable to look up
 * @return value of variable, or NULL if not found
 */
char *nvram_get(const char *varname);

/**
 * nvram_set() - set an nvram variable
 *
 * This sets or deletes the value of an nvram variable. For setting the
 * value the variable is created if it does not already exist.
 *
 * @varname: Variable to adjust
 * @value: Value to set for the variable, or NULL or "" to delete the variable
 * @return 0 if OK, 1 on error
 */
int nvram_set(const char* varname, const char* value);

/**
 * nvram_commit() - commit nvram variables to flash
 *
 * @return 0 if ok, -errno on error
 */
int nvram_commit(void);

/**
 * nvram_get_list() - Get nvram_list
 * @return NULL if not OK
 */
struct nvram_list* const nvram_get_list(void);

#endif // __DR_NVRAM_H__
