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
#include "linux/slab.h"
#include <linux/mutex.h>
#include "linux/string.h"
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;
int total_buffer_size = 0; // for llseek size 

MODULE_AUTHOR("Sachin Mathad"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev; 
    PDEBUG("open");

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

    filp->private_data = dev; 
    /**
     * TODO: handle open
     */
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
    ssize_t retval = 0;
    ssize_t bytes_read = 0;
    size_t entry_offset = 0;

    struct aesd_dev* device = filp->private_data; 
    struct aesd_buffer_entry* buffer_entry; 
    // rx buf null check 
    if(buf == NULL) {
        goto leave;;
    }
    
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    if (mutex_lock_interruptible(&aesd_device.file_lock) != 0){

		printk(KERN_ALERT "Mutex fail\n");
		return -ERESTARTSYS;
	}

    buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&device->circular_buffer, *f_pos, &entry_offset);

    if(buffer_entry == NULL) {
        buf = 0;
        *f_pos = 0;
        goto leave;
    }

    bytes_read = buffer_entry->size - entry_offset;

    if(copy_to_user( buf, buffer_entry->buffptr + entry_offset, bytes_read )!= 0)
	{
		printk(KERN_ALERT "copy fail\n");
		retval = -EFAULT;
        goto leave;
	}

	*f_pos += bytes_read;
    retval = bytes_read;

    leave: mutex_unlock(&aesd_device.file_lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *device = filp->private_data; // append to this struct 
    struct aesd_buffer_entry aesd_entry; // this entry will be stored with kmallocd aesd_dev elements
    char* check_to_free_overwrite;
    int i = 0;
    // unless there is error return count
    ssize_t retval = count;
    
    // rx buf null check 
    if(buf == NULL) {
        goto write_leave;
    }

    if (mutex_lock_interruptible(&aesd_device.file_lock) != 0) {
	    printk(KERN_ALERT "Mutex fail\n");
	    return -ERESTARTSYS;   
	}

    if(device->ele_size == 0) {
        if((device->ele_buffer_ptr = (char *) kmalloc(count, GFP_KERNEL)) == NULL) {
            retval = -EFAULT;
		    goto write_leave;
        }
        device->ele_size = count;
        
        if (copy_from_user(device->ele_buffer_ptr, buf, count)) {
            retval = -EFAULT;
		    goto write_leave;
	    }
    } else {
        if((device->ele_buffer_ptr = (char *) krealloc(device->ele_buffer_ptr,device->ele_size + count, GFP_KERNEL)) == NULL) {
            retval = -EFAULT;
		    goto write_leave;
        }    
        
        if (copy_from_user(device->ele_buffer_ptr + device->ele_size , buf, count)) {
            retval = -EFAULT;
		    goto write_leave;
	    }

        device->ele_size += count;
    }

    for(i = 0; i < device->ele_size; i++) {

        if(device->ele_buffer_ptr[i] == '\n') {
           aesd_entry.size = i + 1; 
           aesd_entry.buffptr =  device->ele_buffer_ptr;

            check_to_free_overwrite = (char *)aesd_circular_buffer_add_entry(&device->circular_buffer, &aesd_entry, &total_buffer_size);

            if(check_to_free_overwrite != NULL) {
                kfree(check_to_free_overwrite);
            }
            device->ele_size = 0;
        } else {
            retval = count;
        }

    }

    write_leave: mutex_unlock(&aesd_device.file_lock); 
        
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
    */

    return retval;
}

// update fpos with the cmd_off + number of bytes in the buffer. 
long aesd_adjust_file_offset(struct file *filp, unsigned int cmd, unsigned int cmd_offset)
{
    struct aesd_dev *device = filp->private_data; // check for values from this struct 
    unsigned int offset_counter = 0;
    long status = 0;
    int i;


    // check for valid cmd number
    if(cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        PDEBUG("Command number too large\n");
        status = -EINVAL;
        goto leave_func;
    }

    // acquire mutex to access circular buffer 
    if (mutex_lock_interruptible(&aesd_device.file_lock) != 0) {

		PDEBUG("Mutex fail\n");
		status = -ERESTARTSYS;
        goto leave_func;
	}

    // check for valid offset 
    if(device->circular_buffer.entry[cmd].size < cmd_offset) {
        PDEBUG("cmd offset fail\n");
        status = -EINVAL;
        goto unlock_leave;
    }

    for(i = 0; i < cmd ; i++) {
        // if null, error or do not account for size?
        if(device->circular_buffer.entry[i].buffptr == NULL) {
            continue;
        }
        offset_counter += device->circular_buffer.entry[i].size;
    }

    // once outside add offset 
    offset_counter += cmd_offset;

    // update fpos 
    filp->f_pos = offset_counter;

    unlock_leave:
    // mutex unlock 
    mutex_unlock(&aesd_device.file_lock);

    leave_func: 
    return status;
}

// based on scull ioctl design 
long aesd_ioctl_unlocked (struct file *filp, unsigned int cmd, unsigned long arg) 
{
    long status = 0;
    struct aesd_seekto copy_user_seek;
    
    //check for valid filp 
    if(filp == NULL) {
        PDEBUG("filp is null\n");
        status = -EINVAL;
        goto ioctl_leave;
    }
    
    /*
        * from text
        * extract the type and number bitfields, and don't decode
        * wrong cmds: return ENOTTY (inappropriate ioctl)
    */

    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) {
        PDEBUG("AESD_IOC_MAGIC fail\n");
        status = -ENOTTY;
        goto ioctl_leave;
    } 

    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) {
        PDEBUG("AESDCHAR_IOC_MAXNR fail\n");
        status = -ENOTTY;
        goto ioctl_leave;
    }

    if(cmd != AESDCHAR_IOCSEEKTO) {
        PDEBUG("ioctl func cmd fail\n");
        status = -EINVAL;
        goto ioctl_leave;
    }

    switch(cmd) {

       case AESDCHAR_IOCSEEKTO:  
       {
            // copy from user 
            if (0 != (copy_from_user(&copy_user_seek, (const void __user*)arg, sizeof(struct aesd_seekto)))) {
                status = -EFAULT;
                goto ioctl_leave;
            } else { 
                status = aesd_adjust_file_offset(filp, copy_user_seek.write_cmd, copy_user_seek.write_cmd_offset);
            }
       } 
       break;
    // default case not required due to check for valid commmand above 
    }

    ioctl_leave: 

    return status;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence) 
{
    int status = 0;
    // total size is updated after every add entry
    status = fixed_size_llseek(filp, off, whence, total_buffer_size);
    return status;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    // own implementation of ioctl called from socket_driver upon special commmand to seek
    .unlocked_ioctl = aesd_ioctl_unlocked,
    // llseek 
    .llseek = aesd_llseek,
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
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    mutex_init(&aesd_device.file_lock);
    /**
     * TODO: initialize the AESD specific portion of the device
     */
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circular_buffer, index) {
      kfree(entry->buffptr);
    }

    mutex_destroy(&aesd_device.file_lock);
    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
