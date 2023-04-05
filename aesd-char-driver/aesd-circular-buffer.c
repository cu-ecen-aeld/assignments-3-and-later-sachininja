/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
   // calculate the total size of the array 
   int array_str_len = 0;
   int req_len = char_offset;
   // start reading from current out_offs file
   int curr_read = buffer->out_offs;
   // set found to false until the required char offset is found
   bool found = false;  
   // run through the buffer 
   int i =0;	
   for( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {

    // add until greater than chat_offset
    array_str_len += buffer->entry[curr_read].size;

    // if array str len is greater than char offset, the char offset is found in the current out_offs
    if(char_offset < array_str_len ) {
        found = true;
        break;
    }
    // reduce lenth to find the right offset 
    req_len -= buffer->entry[curr_read].size;
    // increment current read 	   
    curr_read++;
 
    if(curr_read >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
	    curr_read = 0;
   }
   // if found, set the offset based on req len and return the buffer entry 
   if(found == true) {
    *entry_offset_byte_rtn = req_len;
    return &buffer->entry[curr_read];
   }
   // else not found return null 
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
char * aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{

    char *ret_ptr = NULL;
   // if buffer is full, increment read pointer 
   if(buffer->full) {
    
    // to free the buffer, return the overwritten buffer ptr
    ret_ptr = (char *)buffer->entry[buffer->out_offs].buffptr;

    // reduce the total size by the size that was lost dur to overwrite
    buffer->total_size -= buffer->entry[buffer->out_offs].size;
    if((buffer->out_offs + 1) == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        buffer->out_offs = 0; // index 0
    } else {
        buffer->out_offs += 1;
    }
    
   }
    // store values in the current position of write buffer 
    buffer->entry[buffer->in_offs].buffptr =  add_entry->buffptr;
    buffer->entry[buffer->in_offs].size =  add_entry->size;
    // add to the total size count 
    buffer->total_size += add_entry->size;

    // increment read pointer 
    if((buffer->in_offs + 1) == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        buffer->in_offs = 0; // index 0
    } else {
        buffer->in_offs += 1;
    }

    // check if this filled the buffer by comparing in and out ptr 
    if(buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    }

    return ret_ptr;
}
unsigned int update_total_size(struct aesd_circular_buffer *buffer) 
{
    return buffer->total_size;
}
/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
