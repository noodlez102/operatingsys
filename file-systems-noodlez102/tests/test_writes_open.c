#include "../fs.h"
#include <assert.h>
#include <stdio.h>

void test_create_delete() {
    const char *disk_name = "test_fs";
    const char *file_name = "test_file";

    remove(disk_name); // remove disk if it exists
    assert(make_fs(disk_name) == 0);
    assert(mount_fs(disk_name) == 0);

    // Test creating and deleting files
    assert(fs_create(file_name) == 0);
    assert(fs_create(file_name) == -1); // Creating the same file again should fail
    assert(fs_open(file_name) ==0);
    assert(fs_delete(file_name) == -1); // Deleting an open file should fail

    // Open the file and close it
    int fd = fs_open(file_name);
    assert(fd >= 0);
    assert(fs_close(fd) == 0);

    // Now deletion should succeed
    assert(fs_delete(file_name) == 0);

    // Deleting a non-existent file should fail
    assert(fs_delete(file_name) == -1);

    // Create multiple files and delete them
    for (int i = 0; i < 10; i++) {
        char filename[20];
        sprintf(filename, "file%d", i);
        assert(fs_create(filename) == 0);
        assert(fs_delete(filename) == 0);
    }

    // Unmount and clean up
    assert(umount_fs(disk_name) == 0);
    assert(remove(disk_name) == 0);
}

int main() {
    test_create_delete();
    printf("All tests passed!\n");
    return 0;
}
