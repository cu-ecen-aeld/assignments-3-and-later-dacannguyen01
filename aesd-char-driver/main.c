/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h> // file_operations

#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Hyoun Cho"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;


int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev;

    PDEBUG("OPEN");

    /* TODO: handle open */

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

	filp->private_data = dev;

	/* TODO-END */

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("RELEASE");

    /* TODO: handle release */
	/* TODO-END */

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    //ssize_t retval = 0;
	struct aesd_dev *dev;
	struct aesd_buffer_entry *entry;
	ssize_t read_offs;
	ssize_t  n_read, not_copied, to_read, just_copied, search_offset;
	int i; 

    PDEBUG("READ >>  %zu bytes with offset %lld, f_pos=%lld", count, *f_pos, filp->f_pos);

    /* TODO: handle read  */

	if (filp == NULL) return -EFAULT;

	dev = (struct aesd_dev*) filp->private_data; // circular buffer

	if (dev == NULL) return -EFAULT;

	if (mutex_lock_interruptible(&dev->lock)) {
		return -ERESTARTSYS;
	}

	// return the most recent 10 write commands 
	search_offset = *f_pos; 
	n_read = 0;

	for (i=0; i < 10; i++) {

		entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, search_offset, &read_offs);

		if (entry == NULL) break;

		to_read = entry->size - read_offs;    // rest of the content in the buffer
		if (to_read > count) to_read = count; // read partial
	
		not_copied = copy_to_user(buf + n_read, entry->buffptr + read_offs, to_read);

		just_copied = to_read - not_copied;

		n_read += just_copied;
		count -= just_copied;
		search_offset += just_copied;

		if (count == 0) break;
	}

	*f_pos += n_read;

	// end of mutex
	mutex_unlock(&dev->lock);

	//printk(KERN_ALERT "Total Read = %d", n_read);

	return n_read;

	/* TODO-END */
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    //ssize_t retval = -ENOMEM;
	struct aesd_dev *dev;
	int rc;
	ssize_t n_written;

    PDEBUG("WRITE >>  %zu bytes with offset %lld",count,*f_pos);

    /* TODO: handle write */

	if (filp == NULL) return -EFAULT;

	dev = (struct aesd_dev*) filp->private_data; // circular buffer

	if (mutex_lock_interruptible(&dev->lock)) {
		return -ERESTARTSYS;
	}

	if (dev->working.size == 0) {
		dev->working.buffptr = (char *) kmalloc(count, GFP_KERNEL);
	} else {
		dev->working.buffptr = (char *) krealloc(dev->working.buffptr, dev->working.size + count, GFP_KERNEL);
	}
	
	if (dev->working.buffptr == NULL) {
		n_written = -ENOMEM;
	} else {
		rc = copy_from_user((void *) (dev->working.buffptr + dev->working.size), buf, count);
		n_written = count - rc;	

		dev->working.size += n_written;

		// if there is '\n',
		if (dev->working.buffptr[dev->working.size-1] == '\n') {

			if (dev->buffer.full) {
				kfree(dev->buffer.entry[dev->buffer.out_offs].buffptr);
			}

			aesd_circular_buffer_add_entry(&dev->buffer, &dev->working);

			dev->working.buffptr = NULL;
			dev->working.size = 0;
		}
	}
	
	mutex_unlock(&dev->lock);
	return n_written; // can be error value too

	/* TODO-END */
}

// loff_t aesd_seek(struct file *filp, loff_t off, int whence)
// {
// 	struct aesd_dev *dev;
// 	loff_t newpos;

// 	//PDEBUG("seek %lld from %zu", off, whence);

// 	if (filp == NULL) return -EFAULT;

// 	dev = (struct aesd_dev*) filp->private_data; 

// 	if (mutex_lock_interruptible(&dev->lock)) {
// 	    return -ERESTARTSYS;
// 	}

// 	newpos = fixed_size_llseek(filp, off, whence, dev->buffer.size);

// 	PDEBUG("SEEK > new pos=%lld", newpos);

// 	if (newpos < 0) return -EINVAL;

// 	filp->f_pos = newpos;

// 	mutex_unlock(&dev->lock);

// 	return newpos;
// }

// long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
// {
// 	struct aesd_dev *dev;
// 	struct aesd_seekto cmd_arg;
// 	int result, rc, pos, i;

// 	//PDEBUG("ioctl command=%zu, arg=%lld", cmd, arg);

// 	dev = (struct aesd_dev*) filp->private_data;

// 	if (mutex_lock_interruptible(&dev->lock)) {
// 		return -ERESTARTSYS;
// 	}

// 	switch(cmd) {
// 		case AESDCHAR_IOCSEEKTO:
// 			// decode user command arg
// 			cmd_arg.write_cmd = -1; // default in case
// 			rc = copy_from_user(&cmd_arg, (const void __user *)arg, sizeof(cmd_arg));

// 			if (rc != 0) {
// 				result = -EFAULT;
// 			} else {
// 				// cmd_arg.write_cmd -> command
// 				// cmd_arg.write_cmd_offset -> offset

// 				if (cmd_arg.write_cmd < 0 
// 					|| cmd_arg.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
// 					|| dev->buffer.entry[cmd_arg.write_cmd].buffptr == NULL
// 					|| dev->buffer.entry[cmd_arg.write_cmd].size <= cmd_arg.write_cmd_offset) {
// 					result = -EINVAL;
// 				} else {
// 					pos = 0;

// 					for (i = 0; i < cmd_arg.write_cmd; i++) {
// 						pos += dev->buffer.entry[i].size;
// 					}

// 					// set f_pos
// 					filp->f_pos = pos + cmd_arg.write_cmd_offset;
// 					PDEBUG("IOCTL >> cmd=%d, offset=%d, pos=%lld", 
// 						cmd_arg.write_cmd, cmd_arg.write_cmd_offset, filp->f_pos);

// 					result = 0;
// 				}
// 			}
// 			break;
// 		default:
// 			result = -EINVAL;
// 	}

// 	mutex_unlock(&dev->lock);

// 	return result;
// }

struct file_operations aesd_fops = {
    .owner  = THIS_MODULE,
    .read   = aesd_read,
    .write  = aesd_write,
    .open   = aesd_open,
	// .llseek = aesd_seek,
	// .unlocked_ioctl  = aesd_ioctl,
    .release= aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);

    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;

    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

	printk(KERN_ALERT "Hello, this aesd char driver");

	// dynamic allocation of device number
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);

    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

	// initialize data
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /* TODO: initialize the AESD specific portion of the device */

	// intialize data
	mutex_init(&aesd_device.lock);   // mutex init
	aesd_circular_buffer_init(&aesd_device.buffer); // circular buffer (set zeroes)
	aesd_device.working.size = 0;                   // working buffer

	/* TODO-END */

	// device setup
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
	int i;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /* TODO: cleanup AESD specific poritions here as necessary */

	// free working buffer
	if (aesd_device.working.size > 0) {
		kfree(aesd_device.working.buffptr);
		aesd_device.working.size = 0;
	}

	// free circular buffer
	for(i=0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
		if (aesd_device.buffer.entry[i].size > 0) {
			kfree(aesd_device.buffer.entry[i].buffptr);
			aesd_device.buffer.entry[i].size = 0;
		}
	}

	/* TODO-END */

	// unregister device
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);