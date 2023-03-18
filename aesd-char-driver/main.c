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
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

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
    struct aesd_buffer_entry aesd_entry = NULL; // this entry will be stored with kmallocd aesd_dev elements
    int terminator_rx = 0; 
    char* check_to_free_overwrite = NULL;
    
    ssize_t retval = -ENOMEM;


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

    for(int i = 0; i < device->ele_size; i++) {

        if(device->ele_buffer_ptr[i] == '\n') {
           aesd_entry.size = i + 1; 
           aesd_entry.buffptr =  device->ele_buffer_ptr;

            check_to_free_overwrite = aesd_circular_buffer_add_entry(&device->circular_buffer, &aesd_entry);

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
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
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

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.aesd_circular_buffer, index) {
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
