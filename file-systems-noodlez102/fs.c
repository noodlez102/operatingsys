#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "stdbool.h"
#include "fs.h"
#include <stdint.h>
#include "disk.h"
#include <limits.h>

#define MAX_FILE_NAME 15
#define MAX_NUM_OF_FILES 64
#define TOTAL_BLOCKS 8192
#define MAX_FILESIZE (1 << 20)
#define BLOCK_SIZE 4096 
#define MAX_FILE_DESCRIPTORS 32

struct super_block {
    uint16_t used_block_bitmap_count;
    uint16_t used_block_bitmap_offset;
    uint16_t inode_metadata_blocks;
    uint16_t inode_metadata_offset;
};

struct inode { //not all of these is needed
    unsigned long int file_type;
    unsigned long int direct_offset[MAX_FILESIZE/BLOCK_SIZE];
    unsigned long int single_indirect_offset;
    unsigned long int double_indirect_offset;
    unsigned long int file_size;
};

struct dir_entry {
    bool is_used;
    int inode_number;
    char name[MAX_FILE_NAME];
};

struct file_descriptor {
    bool is_used;
    int inode_number;
    int offset;
};

//global variables
int open_disk_fd=-1;
uint8_t used_block_bitmap[TOTAL_BLOCKS / CHAR_BIT]; //the code given in hints
struct inode inode_table[MAX_NUM_OF_FILES];
struct file_descriptor open_file_descriptors[MAX_FILE_DESCRIPTORS];
struct super_block superblock;
struct dir_entry directory[MAX_NUM_OF_FILES];
uint8_t inode_bitmap[MAX_NUM_OF_FILES]; // possibly useless
//**********************************************************//
//helper functions for bitmap
void set_bit(uint8_t* bitmap, size_t index) {
    bitmap[index / 8] |= (1 << (index % 8));
}

void clear_bit(uint8_t* bitmap, size_t index) {
    bitmap[index / 8] &= ~(1 << (index % 8));
}

bool get_bit(uint8_t* bitmap, size_t index) {
    return (bitmap[index / 8] & (1 << (index % 8))) != 0;
}
//*************************************************************//

int make_fs(const char *disk_name) {

    if (make_disk(disk_name) != 0)
    {
        return -1;
    }

    if (open_disk(disk_name) != 0)
    {
        return -1;
    }

    superblock.used_block_bitmap_count = (TOTAL_BLOCKS / CHAR_BIT + BLOCK_SIZE - 1) / BLOCK_SIZE;
    superblock.used_block_bitmap_offset = 1;
    superblock.inode_metadata_blocks = (sizeof(struct inode) * MAX_NUM_OF_FILES + BLOCK_SIZE - 1) / BLOCK_SIZE;
    superblock.inode_metadata_offset = superblock.used_block_bitmap_offset + superblock.used_block_bitmap_count;

    //Initialize block bitmap
    for (int i = 0; i < superblock.inode_metadata_offset; i++) {
        if (i < superblock.used_block_bitmap_offset)
        {
           used_block_bitmap[i] = 1;
        }
        else
        {
           used_block_bitmap[i] = 0; 
        }
    }
    memset(used_block_bitmap, 0, sizeof(used_block_bitmap)); //idk man
    memset(inode_bitmap, 0, sizeof(inode_bitmap));
    // Write superblock to disk
    if (block_write(0, &superblock) != 0) {
        return -1;
    }

    // Write block bitmap to disk
    if (block_write(superblock.used_block_bitmap_offset,used_block_bitmap) != 0) {
        return -1;
    }

    // Close disk
    if (close_disk() != 0) {
        return -1;
    }
    return 0;
}

int mount_fs(const char *disk_name) {

    if (disk_name == NULL) {
        return -1;
    }
    if (open_disk(disk_name) == -1) {
        return -1;
    }

    char *block_holder = calloc(1, BLOCK_SIZE);

    if (block_read(0, block_holder) == -1) {
        free(block_holder);
        close_disk();
        return -1;
    }

    memcpy((void *)&superblock, (void *)block_holder, sizeof(struct super_block));

    if (block_read(superblock.inode_metadata_offset + superblock.inode_metadata_blocks, block_holder) == -1) {
        free(block_holder);
        close_disk();
        return -1;
    }

    memcpy((void *)directory, (void *)block_holder, sizeof(struct dir_entry) * MAX_NUM_OF_FILES);

    // Reading the Inode table info
    for (int i = 0; i < superblock.inode_metadata_blocks; i++) {
        if (block_read(superblock.inode_metadata_offset + i, block_holder) == -1) {
            free(block_holder);
            close_disk();
            return -1;
        }
        memcpy((void *)(inode_table + i * (BLOCK_SIZE / sizeof(struct inode))), (void *)block_holder, BLOCK_SIZE);
    }

    // Reading the Bitmap
    if (block_read(superblock.used_block_bitmap_offset, block_holder) == -1) {
        free(block_holder);
        close_disk();
        return -1;
    }

    memcpy(used_block_bitmap, block_holder, sizeof(char) * superblock.used_block_bitmap_count);

    // Initialising the File Descriptor array info
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        open_file_descriptors[i].is_used = false;
        open_file_descriptors[i].inode_number = -1;
        open_file_descriptors[i].offset = 0;
    }
    open_disk_fd=0;
    free(block_holder);
    return 0;
}

int umount_fs(const char *disk_name) {

    if (disk_name == NULL) {
        return -1;
    }

    // Create a buffer for writing
    char *block_holder = calloc(1, BLOCK_SIZE);

    // Writing the Super Block info
    memcpy((void *)block_holder, (void *)&superblock, sizeof(struct super_block));
    if (block_write(0, block_holder) == -1) {
        free(block_holder);
        close_disk();
        return -1;
    }

    // Writing the Directory info
    memcpy((void *)block_holder, (void *)directory, sizeof(struct dir_entry) * MAX_NUM_OF_FILES);
    if (block_write(superblock.inode_metadata_offset + superblock.inode_metadata_blocks, block_holder) == -1) {
        free(block_holder);
        close_disk();
        return -1;
    }

    // Writing the Inode Table info
    for (int i = 0; i < superblock.inode_metadata_blocks; i++) {
        memcpy((void *)block_holder, (void *)(inode_table + i * (BLOCK_SIZE / sizeof(struct inode))), BLOCK_SIZE);
        if (block_write(superblock.inode_metadata_offset + i, block_holder) == -1) {
            free(block_holder);
            close_disk();
            return -1;
        }
    }

    // Zero out the File Descriptor array
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (open_file_descriptors[i].is_used) {
            open_file_descriptors[i].is_used = false;
            open_file_descriptors[i].inode_number = -1;
            open_file_descriptors[i].offset = 0;
        }
    }

    free(block_holder);

    // Close the disk
    if (close_disk() == -1) {
        return -1;
    }
    open_disk_fd=-1;
    return 0;
}

int fs_open(const char *name) {

    if (open_disk_fd == -1) {
        fprintf(stderr, "Disk is not mounted\n");
        return -1;
    }
    int fildes = -1;

    // Find the file in the directory with the matching name
    for (int i = 0; i < MAX_NUM_OF_FILES; i++) {
        if (directory[i].is_used && strncmp(name, directory[i].name, MAX_FILE_NAME) == 0) {
            // Find an unused file descriptor
            for (int j = 0; j < MAX_FILE_DESCRIPTORS; j++) {
                if (!open_file_descriptors[j].is_used) {
                    fildes = j;
                    break;
                }
            }

            if (fildes == -1) {
                return -1; // All file descriptors are in use
            }
            
            // Open the file
            printf("File is now open\n");
            open_file_descriptors[fildes].is_used = true;
            open_file_descriptors[fildes].inode_number = directory[i].inode_number;
            open_file_descriptors[fildes].offset = 0;
            return fildes;
        }
    }
    return -1;
}

int fs_close(int fildes) {
    // Implementation here
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS) {
        fprintf(stderr, "Invalid file\n");
        return -1;
    }
    if (!open_file_descriptors[fildes].is_used) {
        fprintf(stderr, "File descriptor is not open\n");
        return -1;
    }

    open_file_descriptors[fildes].is_used = false;
    open_file_descriptors[fildes].inode_number = 0;
    open_file_descriptors[fildes].offset = 0;
    printf("file is now closed\n");
    return 0;
}

int fs_create(const char *name) {
    // Check if the file name is too long
    if (strlen(name) > MAX_FILE_NAME || strlen(name)==0) {
        fprintf(stderr, "File name invalid\n");
        return -1;
    }

    if (open_disk_fd == -1) {
        fprintf(stderr, "Disk is not mounted\n");
        return -1;
    }
    // Check if the file already exists
    for (int i = 0; i < MAX_NUM_OF_FILES; i++) {
        if (directory[i].is_used && strncmp(directory[i].name, name, MAX_FILE_NAME) == 0) {
            fprintf(stderr, "File name already exists\n");
            return -1;
        }
    }

    // Find the first available inode
    int available_inode = -1;
    for (int i = 0; i < MAX_NUM_OF_FILES; i++) {
        if (inode_table[i].file_type == 0) {
            available_inode = i;
            break;
        }
    }

    // If there are no available inodes, return -1
    if (available_inode == -1) {
        fprintf(stderr, "Reached File limit\n");
        return -1;
    }

    // Initialize the new inode
    inode_table[available_inode].file_type = 1;
    inode_table[available_inode].file_size = 0;
    memset(inode_table[available_inode].direct_offset, 0, sizeof(inode_table[available_inode].direct_offset));
    inode_table[available_inode].single_indirect_offset = 0;
    inode_table[available_inode].double_indirect_offset = 0;
    set_bit(inode_bitmap, available_inode); // Mark the inode as used

    // Find the first available directory entry
    int available_dir_entry = -1;
    for (int i = 0; i < MAX_NUM_OF_FILES; i++) {
        if (!directory[i].is_used) {
            available_dir_entry = i;
            break;
        }
    }

    // If there are no available directory entries, return -1
    if (available_dir_entry == -1) {
        fprintf(stderr, "No available directory entries\n");
        return -1;
    }

    // Update the directory entry
    directory[available_dir_entry].is_used = true;
    directory[available_dir_entry].inode_number = available_inode;
    strncpy(directory[available_dir_entry].name, name, MAX_FILE_NAME);

    printf("fs_create complete!\n");
    return 0;
}

int fs_delete(const char *name) {
    // Implementation here
    if (open_disk_fd == -1) {
        fprintf(stderr, "fs_delete: File system not mounted.\n");
        return -1;
    }

    // Find the file in the directory with the matching name
    int file_index = -1;
    for (int i = 0; i < MAX_NUM_OF_FILES; i++) {
        if (directory[i].is_used && strncmp(directory[i].name, name, MAX_FILE_NAME) == 0) {
            file_index = i;
            break;
        }
    }

    // If the file does not exist, return -1
    if (file_index == -1) {
        fprintf(stderr, "fs_delete: File '%s' does not exist.\n", name);
        return -1;
    }

    // Check if the file is currently open
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (open_file_descriptors[i].is_used && open_file_descriptors[i].inode_number == directory[file_index].inode_number) {
            fprintf(stderr, "fs_delete: File '%s' is currently open.\n", name);
            return -1;
        }
    }

    // Mark all the blocks used by the file as available
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (used_block_bitmap[i] == directory[file_index].inode_number) {
            clear_bit(used_block_bitmap, i); // Mark as available
            superblock.used_block_bitmap_count--;
        }
    }

    // Mark the directory entry as unused
    directory[file_index].is_used = false;
    directory[file_index].inode_number = 0;
    memset(directory[file_index].name, 0, MAX_FILE_NAME);

    return 0;
}

int fs_read(int fildes, void *buf, size_t nbyte) {
    // Implementation here
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !open_file_descriptors[fildes].is_used) {
        fprintf(stderr, "fs_read: Invalid file descriptor.\n");
        return -1;
    }

    int file_index = open_file_descriptors[fildes].inode_number;
    if (file_index < 0 || file_index >= MAX_NUM_OF_FILES || !directory[file_index].is_used) {
        fprintf(stderr, "fs_read: Invalid file associated with the file descriptor.\n");
        return -1;
    }

    int remaining_bytes = inode_table[file_index].file_size - open_file_descriptors[fildes].offset;
    int bytes_to_read = (nbyte < remaining_bytes) ? nbyte : remaining_bytes;
    if (bytes_to_read <= 0) {
        return 0;
    }

    int block_index = open_file_descriptors[fildes].offset / BLOCK_SIZE;
    int block_offset = open_file_descriptors[fildes].offset % BLOCK_SIZE;
    int bytes_read = 0;
    int bytes_left = bytes_to_read;
    char *block_buffer = (char *)calloc(1, BLOCK_SIZE);

    while (bytes_left > 0) {
        if (block_read(inode_table[file_index].direct_offset[block_index], block_buffer) == -1) {
            fprintf(stderr, "fs_read: Data could not be read from disk.\n");
            free(block_buffer);
            return -1;
        }
        int current_block_bytes = (bytes_left < BLOCK_SIZE - block_offset) ? bytes_left : BLOCK_SIZE - block_offset;
        memcpy((char *)buf + bytes_read, block_buffer + block_offset, current_block_bytes);
        bytes_read += current_block_bytes;
        bytes_left -= current_block_bytes;
        block_index++;
        block_offset = 0;
    }

    free(block_buffer);
    open_file_descriptors[fildes].offset += bytes_read;
    return bytes_read;
}

int fs_write(int fildes, void *buf, size_t nbyte) {

    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !open_file_descriptors[fildes].is_used) {
        fprintf(stderr, "fs_write: Invalid file descriptor\n");
        return -1;
    }

    int file_index = open_file_descriptors[fildes].inode_number;
    if (file_index < 0 || file_index >= MAX_NUM_OF_FILES || !directory[file_index].is_used) {
        fprintf(stderr, "fs_write: Invalid file associated with the file descriptor\n");
        return -1;
    }

    int bytes_written = 0;
    int block_index = open_file_descriptors[fildes].offset / BLOCK_SIZE;
    int block_offset = open_file_descriptors[fildes].offset % BLOCK_SIZE;
    char *block_buffer = calloc(1, BLOCK_SIZE);

    while (bytes_written < nbyte) {
        if (inode_table[file_index].direct_offset[block_index] == 0) {
            // Allocate a new block
            for (int i = superblock.used_block_bitmap_offset; i < TOTAL_BLOCKS; i++) {
                if (!get_bit(used_block_bitmap, i)) {
                    set_bit(used_block_bitmap, i);
                    inode_table[file_index].direct_offset[block_index] = i;
                    superblock.used_block_bitmap_count++;
                    break;
                }
                if (i == TOTAL_BLOCKS - 1) {
                    fprintf(stderr, "fs_write: Disk is full\n");
                    free(block_buffer);
                    return -1;
                }
            }
        }

        // Read the block
        if (block_read(inode_table[file_index].direct_offset[block_index], block_buffer) == -1) {
            fprintf(stderr, "fs_write: Error reading block from disk\n");
            free(block_buffer);
            return -1;
        }

        // Write the data to the block
        int bytes_to_write = (nbyte - bytes_written < BLOCK_SIZE - block_offset) ? nbyte - bytes_written : BLOCK_SIZE - block_offset;
        memcpy(block_buffer + block_offset, (char *)buf + bytes_written, bytes_to_write);

        // Write the block back to disk
        if (block_write(inode_table[file_index].direct_offset[block_index], block_buffer) == -1) {
            fprintf(stderr, "fs_write: Error writing block to disk\n");
            free(block_buffer);
            return -1;
        }

        bytes_written += bytes_to_write;
        open_file_descriptors[fildes].offset += bytes_to_write;
        if (open_file_descriptors[fildes].offset > inode_table[file_index].file_size) {
            inode_table[file_index].file_size = open_file_descriptors[fildes].offset;
        }
        block_index++;
        block_offset = 0;
    }

    free(block_buffer);
    return bytes_written;
}

int fs_get_filesize(int fildes) {
    // Implementation here
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !open_file_descriptors[fildes].is_used) {
        fprintf(stderr, "fs_get_filesize: Invalid file descriptor\n");
        return -1;
    }

    for (int i = 0; i < MAX_NUM_OF_FILES; i++) {
        if (directory[i].is_used && directory[i].inode_number == open_file_descriptors[fildes].inode_number) {
            return inode_table[directory[i].inode_number].file_size;
        }
    }

    fprintf(stderr, "fs_get_filesize: File not found\n");
    return -1;
}

int fs_listfiles(char ***files) {
        int num_of_files = 0;

    // Count the number of files present
    for (int i = 0; i < MAX_NUM_OF_FILES; i++) {
        if (directory[i].is_used) {
            num_of_files++;
        }
    }

    // Allocate space for the file names
    *files = (char **)calloc(num_of_files + 1, sizeof(char *));
    if (*files == NULL) {
        return -1;
    }

    // Allocate space for each filename and copy the names
    for (int i = 0; i < num_of_files; i++) {
        (*files)[i] = (char *)calloc(MAX_FILE_NAME + 1, sizeof(char));
        if ((*files)[i] == NULL) {
            // Free previously allocated memory and return error
            for (int j = 0; j < i; j++) {
                free((*files)[j]);
            }
            free(*files);
            return -1;
        }
        strncpy((*files)[i], directory[i].name, MAX_FILE_NAME);
    }

    // Terminate the array with a NULL pointer
    (*files)[num_of_files] = NULL;

    return 0;
}

int fs_lseek(int fildes, off_t offset) {
    // Implementation here
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !open_file_descriptors[fildes].is_used) {
        fprintf(stderr, "fs_lseek: Invalid file descriptor.\n");
        return -1;
    }

    if (offset < 0) {
        fprintf(stderr, "fs_lseek: Invalid offset (less than zero).\n");
        return -1;
    }

    int file_index = open_file_descriptors[fildes].inode_number;
    if (file_index < 0 || file_index >= MAX_NUM_OF_FILES || !directory[file_index].is_used) {
        fprintf(stderr, "fs_lseek: Invalid file associated with the file descriptor.\n");
        return -1;
    }

    if (offset > inode_table[file_index].file_size) {
        fprintf(stderr, "fs_lseek: Requested offset is larger than the file size.\n");
        return -1;
    }

    open_file_descriptors[fildes].offset = offset;
    return 0;
}

int fs_truncate(int fildes, off_t length) {
    // Implementation here
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !open_file_descriptors[fildes].is_used) {
        fprintf(stderr, "fs_truncate: Invalid file descriptor.\n");
        return -1;
    }

    int file_index = open_file_descriptors[fildes].inode_number;
    if (file_index < 0 || file_index >= MAX_NUM_OF_FILES || !directory[file_index].is_used) {
        fprintf(stderr, "fs_truncate: Invalid file associated with the file descriptor.\n");
        return -1;
    }

    if (length > inode_table[file_index].file_size) {
        fprintf(stderr, "fs_truncate: Requested length is larger than the file size.\n");
        return -1;
    }

    inode_table[file_index].file_size = length;
    open_file_descriptors[fildes].offset = length;
    return 0;
}