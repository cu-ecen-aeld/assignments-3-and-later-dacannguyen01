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
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Nguyen Dac An"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}


int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_buffer_entry *entry;
    ssize_t read_off;
    ssize_t char_offset;
    ssize_t to_read, not_copied, just_copied, n_read;
    
    if (filp == NULL) return -EFAULT; 
    struct aesd_dev *dev = (struct aesd_dev*) filp->private_data;
    
    if (dev == NULL) return -EFAULT;
    
    if (mutex_lock_interruptible(&dev->mu)) return -ERESTARTSYS;
    
    char_offset = *f_pos;

    for (int i=0; i<10; i++)
    {
    	entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buf, char_offset, &read_off);
    	if (entry == NULL) break;
    	to_read = entry->size - read_off;
    	if (to_read > count) to_read = count;
    	not_copied = copy_to_user(buf + n_read, entry->buffptr + read_off, to_read);
    	just_copied = to_read - not_copied;
    	n_read += just_copied;
    	count -= just_copied;
    	char_offset += just_copied;
    	if (count == 0) break;
   	}
    *f_pos += n_read;
    mutex_unlock(&dev->mu);

    return n_read;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev;
    ssize_t n_written;
    int not_copied;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    /* TODO: handle write */

    if (filp == NULL) return -EFAULT;
    dev = (struct aesd_dev*) filp->private_data;
    
    if (dev == NULL) return -EFAULT;
    if (mutex_lock_interruptible(&dev->mu)) return -ERESTARTSYS;

    if(dev->ent.size == 0) {
        dev->ent.buffptr = (char *) kmalloc(count, GFP_KERNEL);
    }
    else {
        dev->ent.buffptr = (char *) krealloc(dev->ent.buffptr, dev->ent.size + count, GFP_KERNEL);
    }

    if (dev->ent.buffptr == NULL) {
        n_written = -ENOMEM;
    }
    else {
        not_copied = copy_from_user((void *) (dev->ent.buffptr + dev->ent.size), buf, count);
        n_written = count - not_copied;

        dev->ent.size += n_written;

        if(dev->ent.buffptr[dev->ent.size-1] == '\n') {
            if (dev->buf.full) {
                kfree(dev->buf.entry[dev->buf.out_offs].buffptr);
            }

            aesd_circular_buffer_add_entry(&dev->buf, &dev->ent);

            dev->ent.buffptr = NULL;
            dev->ent.size = 0;
        }
    }
    
    mutex_unlock(&dev->mu);
  
    return n_written;
}

loff_t aesd_seek(struct file *filp, loff_t f_pos, int whence)
{
	struct aesd_dev *dev;
	loff_t new_pos;
	
	if(filp == NULL) return -EFAULT;
	dev = (struct aesd_dev*) filp->private_data;
	
	if (dev == NULL) return -EFAULT;
    if (mutex_lock_interruptible(&dev->mu)) return -ERESTARTSYS;
    
    new_pos = fixed_size_llseek(filp, f_pos, whence, dev->buf.size);
    PDEBUG("SEEK > new pos =%lld", new_pos);
	if (new_pos < 0) return -EINVAL;
    
    filp->f_pos = new_pos;
    
    mutex_unlock(&dev->mu);
    return new_pos;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct aesd_dev *dev;
	struct aesd_seekto cmd_arg;
    int result, rc, pos;

    if(filp == NULL) return -EFAULT;
    dev = (struct aesd_dev*) filp->private_data;

    if (dev == NULL) return -EFAULT;
    if (mutex_lock_interruptible(&dev->mu)) return -ERESTARTSYS;

    switch(cmd)
    {
        case AESDCHAR_IOCSEEKTO:
            cmd_arg.write_cmd = -1;
            rc = copy_from_user(&cmd_arg, (const void __user*)arg, sizeof(cmd_arg));

            if (rc != 0) 
            {
                result = -EFAULT;
            }
            else 
            {
                if (cmd_arg.write_cmd < 0 ||
                    cmd_arg.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED ||
                    dev->buf.entry[cmd_arg.write_cmd].buffptr == NULL ||
                    dev->buf.entry[cmd_arg.write_cmd].size <= cmd_arg.write_cmd_offset)
                    {
                        result = -EINVAL;
                    }
                else
                {
                    pos = 0;

                    for (int i = 0; i < cmd_arg.write_cmd; i++)
                    {
                        pos += dev->buf.entry[i].size;
                    }

                    filp->f_pos = pos + cmd_arg.write_cmd_offset;
                    PDEBUG("IOCTL >> cmd=%d, offset%d, pos%lld", cmd_arg.write_cmd, cmd_arg.write_cmd_offset, filp->f_pos);
                    result = 0;
                }
            }
            break;
        default:
            result = -EINVAL;
    }

    mutex_unlock(&dev->mu);
    return result;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .llseek =   aesd_seek,
    .unlocked_ioctl = aesd_ioctl,
    .release =  aesd_release,
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

// int aesd_init_module(void)
// {
//     dev_t dev = 0;
//     int result;

// 	printk(KERN_ALERT "Hello, this aesd char driver");

//     result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
//     aesd_major = MAJOR(dev);

//     if (result < 0) {
//         printk(KERN_WARNING "Can't get major %d\n", aesd_major);
//         return result;
//     }
//     memset(&aesd_device,0,sizeof(struct aesd_dev));

//     /**
//      * TODO: initialize the AESD specific portion of the device
//      */
//     aesd_circular_buffer_init(&aesd_device.buf);
//     mutex_init(&aesd_device.mu);
//     aesd_device.ent.size = 0; 

//     result = aesd_setup_cdev(&aesd_device);

//     if( result ) {
//         unregister_chrdev_region(dev, 1);
//     }
//     return result;
// }

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.buf);
    mutex_init(&aesd_device.mu);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    uint8_t index = 0;
	struct aesd_buffer_entry *entry;
	AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buf,index) {
		if(entry->buffptr!=NULL){	
			kfree(entry->buffptr);
		}
	}
    mutex_destroy(&aesd_device.mu);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
