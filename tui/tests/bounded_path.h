#ifndef TRAINLOG_TEST_BOUNDED_PATH_H
#define TRAINLOG_TEST_BOUNDED_PATH_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* CONTRACT: failure leaves output empty and no caller may perform filesystem
 * work with a partial path. Lengths include the separator and trailing NUL. */
static bool
trainlog_test_join_path(char *output, size_t capacity, const char *parent, const char *leaf) {
    size_t parent_length;
    size_t leaf_length;

    if (output == NULL || capacity == 0U || parent == NULL || leaf == NULL) {
        return false;
    }
    output[0] = '\0';
    parent_length = strlen(parent);
    leaf_length = strlen(leaf);
    if (leaf_length > SIZE_MAX - 2U || parent_length > SIZE_MAX - leaf_length - 2U ||
        parent_length + leaf_length + 2U > capacity) {
        return false;
    }

    (void)memcpy(output, parent, parent_length);
    output[parent_length] = '/';
    (void)memcpy(output + parent_length + 1U, leaf, leaf_length);
    output[parent_length + leaf_length + 1U] = '\0';
    return true;
}

#endif
