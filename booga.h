/*
 * booga.h
 *
 *      Author: Joey 15309001
 */

#ifndef __BOOGA_H
#define __BOOGA_H

#define MODULE_USAGE_UP { try_module_get(THIS_MODULE); }
#define MODULE_USAGE_DOWN { module_put(THIS_MODULE); }

#ifndef BOOGA_MAJOR
#define BOOGA_MAJOR 0
#endif

#ifndef BOOGA_NR_DEVS
#define BOOGA_NR_DEVS 4
#endif

/*
 * Split minors in two parts
 */
#define TYPE(dev) (MINOR(dev) >> 4)
#define NUM(dev) (MINOR(dev) & 0xf)

struct booga_device {
	char *str;
	unsigned int number;
	unsigned int usage;
};
typedef struct booga_device booga_device;

struct booga_stats{
	long int bytes_read;
	long int bytes_written;
	booga_device devs[BOOGA_NR_DEVS];
};
typedef struct booga_stats booga_stats;

extern booga_stats Booga_Device_Stats;
#endif /* __BOOGA_H */