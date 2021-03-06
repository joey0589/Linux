/*
 ============================================================================
 Name        : booga.c
 Author      : Joey 15309001
 Version     : 0.1
 Description : booga driver for Linux 
 ============================================================================
 */

#ifndef __KERNEL__
#define __KERNEL__
#endif
#ifndef MODULE
#define MODULE
#endif

#include "booga.h"        /* local definitions */

#include <linux/random.h> /*to generate the random phrase
#include <linux/signal.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/version.h> /* printk() */
#include <linux/init.h>  /*  for module_init and module_cleanup */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/proc_fs.h>  /* size_t */
#include <linux/unistd.h> 
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/signal.h>
#include <asm/uaccess.h>	/* for copy_from_user */

#define PROC_MAX_SIZE	1024
#define PROC_NAME 		"driver/booga"

static int booga_major = BOOGA_MAJOR;
static int booga_nr_devs = BOOGA_NR_DEVS; /* number of bare booga devices */
module_param(booga_major, int, 0);
module_param(booga_nr_devs, int, 0);
MODULE_AUTHOR("Joey");

static booga_stats *booga_device_stats;

uint current_device;
uint random_phrases_count[4];
char * random_phrases[] = {"booga! booga!", "googoo! gaagaa!", "wooga! wooga!", "neka! maka!"};
static booga_stats *booga_device_stats;

static int __init booga_init(void);
static int booga_open(struct inode *inode, struct file *filp);
static ssize_t booga_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static char* get_random_phrase(void);
static ssize_t booga_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static int booga_release(struct inode *inode, struct file *filp);
static int booga_read_procmem (char *buf, char **start, off_t offset,
                                int count, int *eof, void *data);
static void __exit booga_exit(void);

/* The different file operations */

static struct file_operations booga_fops = {
		.llseek	= NULL,
		.read	= booga_read,
		.write	= booga_write,
		.open	= booga_open,
		.release = booga_release,
};

/**
 * This structure hold information about the /proc file
 *
 */
//static struct proc_dir_entry *Proc_File;

/**
 * The buffer used to store character for this module
 *
 */
static char proc_buff[PROC_MAX_SIZE];

/**
 * The size of the buffer
 *
 */
static unsigned long proc_buff_size = 0;

/*
 * Open and close
 */

static int booga_open(struct inode *inode, struct file *filp){
	int num = NUM(inode->i_rdev);
	int minor;
	if (num >= booga_nr_devs) return -ENODEV;
	current_device = num;
	printk("<1>booga_open invoked.%d\n", num);
	booga_device_stats->devs[num].usage++;
	filp->f_op = &booga_fops;

/* need to protect this with a semaphore if multiple processes
	   will invoke this driver to prevent a race condition */
	if (down_interruptible (&booga_device_stats->sem))
		return (-ERESTARTSYS);
		minor = NUM(inode->i_rdev);
    switch(minor) {
	    case 0:
	        booga_device_stats->num_open0++;
	        break;
	    case 1:
	        booga_device_stats->num_open1++;
	        break;
	    case 2:
	        booga_device_stats->num_open2++;
	        break;
	    case 3:
	        booga_device_stats->num_open3++;
	        break;
	}
	
	up(&booga_device_stats->sem);

	MODULE_USAGE_UP;
	return 0;
}
static int booga_release(struct inode *inode, struct file *filp){
/* need to protect this with a semaphore if multiple processes
   	will invoke this driver to prevent a race condition */
	if (down_interruptible (&booga_device_stats->sem))
			return (-ERESTARTSYS);
	booga_device_stats->num_close++; 
	up(&booga_device_stats->sem);

	printk("<1>booga_release invoked.\n");
	MODULE_USAGE_DOWN;
	return 0;
}

/*
 * Data management: read and write
 */
static ssize_t booga_read(struct file *filp, char *buf, size_t count, loff_t *f_pos){
	int result;
	int status;
	char *random;
	char *temp;
	int i;

	printk("<1>booga_read invoked.\n");
	booga_device_stats->devs[current_device].str= (char *) kmalloc(sizeof(char)*count, GFP_KERNEL);
	if (!booga_device_stats->devs[current_device].str) {
		result = -ENOMEM;
		goto fail_malloc;
	}

	random = get_random_phrase();
	temp = random;
	for(i=0; i<count; i++) {
		// if temp is point to the last character...
		if(*temp=='\0') {
			// reset the pointer so that it points to the first character from 'random' str
			temp = random;
			booga_device_stats->devs[current_device].str[i] = ' ';
			continue;
		}
		// dereference temp and copy to dev->str
		booga_device_stats->devs[current_device].str[i] = *temp;
		temp++;
	}

	status = __copy_to_user(buf, booga_device_stats->devs[current_device].str, count);
	if (status > 0)
		printk("simple: Could not copy %d bytes\n", status);

	if(booga_device_stats->devs[current_device].str)
		kfree(booga_device_stats->devs[current_device].str);

	booga_device_stats->bytes_read += count;

	return count;
	fail_malloc:
		return result;
}

static ssize_t booga_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos){
	int minor;
    minor = NUM(filp->f_dentry->d_inode->i_rdev);
    if(minor == 3) { 
        return kill_pid(task_pid(current), SIGTERM, 0);
    }    
        
	printk("<1>booga_write invoked.\n");

	if (down_interruptible (&booga_device_stats->sem))
		return (-ERESTARTSYS);
    booga_device_stats->bytes_written += count;

	up(&booga_device_stats->sem);

	return count;   // pretend that count bytes were written
}

/*
 * Returns a random phrase
 */
static char* get_random_phrase(void){
	char randval;
	uint choice ;
	get_random_bytes(&randval, 1);
	choice = (randval & 0x7F) % 4;
	random_phrases_count[choice]++;
	return random_phrases[choice];
}

static void init_booga_device_stats(void){
	int i;
	booga_device_stats->bytes_read = 0;
	booga_device_stats->bytes_written = 0;

	for(i=0; i<booga_nr_devs; i++){
		booga_device_stats->devs[i].number=0;
		booga_device_stats->devs[i].usage=0;
		random_phrases_count[i] = 0;
	}
}

static int booga_read_procmem(char *buf, char **start, off_t offset, int count, int *eof, void *data){
	int len = 0;
	int i = 0;
	printk("<1> booga device procmem requested\n");
	len = sprintf(buf, "bytes read = %ld\n", booga_device_stats->bytes_read);
	len += sprintf(buf+len, "bytes written = %ld\n", booga_device_stats->bytes_written);
	len += sprintf(buf+len, "number of opens:\n");
	for(i=0; i<booga_nr_devs; i++)
		len += sprintf(buf+len, "\t/dev/booga%d\t= %d times\n", i, booga_device_stats->devs[i].usage);

	len += sprintf(buf+len, "strings outputs:\n");
	for(i=0; i<booga_nr_devs; i++)
		len += sprintf(buf+len, "\t%s\t= %d times\n", random_phrases[i], random_phrases_count[i]);
	return len;
}

static __init int booga_init(void){
	int result;

	/*
	 * Register a major, and accept a dynamic number
	 */
	result = register_chrdev(booga_major, "booga", &booga_fops);
	if(result < 0){
		printk(KERN_WARNING "booga: can't get major %d\n", booga_major);
		return result;
	}

	if(booga_major == 0) booga_major = result;
	printk("<1> booga device driver version 0.1: loaded at major number %d and nr: %d\n", booga_major, booga_nr_devs);

	booga_device_stats = (booga_stats *)kmalloc(sizeof(booga_stats), GFP_KERNEL);

	if(!booga_device_stats){
		result = -ENOMEM;
		goto fail_malloc;
	}

	init_booga_device_stats();
	create_proc_read_entry("driver/booga", 0, NULL, booga_read_procmem, NULL);

	return 0;

	fail_malloc:
		unregister_chrdev(booga_major, "booga");
		remove_proc_entry("driver/booga", NULL);
		return result;
}

static __exit void booga_exit(void){
	remove_proc_entry("driver/booga", NULL);
	kfree(booga_device_stats);
	unregister_chrdev(booga_major, "booga");
	printk("<1> booga device driver version 0.1: unloaded\n");
}

module_init(booga_init);
module_exit(booga_exit);