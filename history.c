#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"

typedef struct {
    char **entries;       // dynamically allocated array of strings
    int *command_nums;   // 🔧 NEW
    int limit;            // max number of entries (100)
    int count;            // keeps count of how many entries are in the buffer
    int start;            // index of the oldest entry (circular buffer)
    int next_cmd_num;     // global command number (starts at 1) //how is this different than count?
} History;

static History hist;

//why are we using calloc?  Because it starts with 0's out?
void hist_init(unsigned int limit) {
    hist.entries = calloc(limit, sizeof(char *)); //does this mean allocate memory for hist.entries for limit # of entries with size of char *?
    hist.command_nums = calloc(limit, sizeof(int));  // room for ints that track command entries?
    hist.limit = limit; 
    hist.count = 0;
    hist.start = 0;
    hist.next_cmd_num = 1;
}

void hist_destroy(void) {
    for (int i = 0; i < hist.limit; i++) {
        free(hist.entries[i]);
    }
    free(hist.entries);
}

void hist_add(const char *cmd) {
    // Ignore empty commands or NULL
    if (!cmd || strlen(cmd) == 0) return;

    int idx;
    if (hist.count < hist.limit) {
        idx = (hist.start + hist.count) % hist.limit; //what is this calculuation??
        hist.count++;
    } else {
        // Overwrite oldest entry
        idx = hist.start;
        free(hist.entries[idx]);
        hist.start = (hist.start + 1) % hist.limit;
    }

    hist.entries[idx] = strdup(cmd);
    hist.command_nums[idx] = hist.next_cmd_num++;  // 🔧 NEW
    //printf("🔹 [hist_add] Stored at index %d: \"%s\" (cmd_num=%d)\n", idx, hist.entries[idx], hist.command_nums[idx]);
    //printf for debugging, consider removal
}

void hist_print(void) {
    int cmd_num_start = hist.next_cmd_num - hist.count;
    for (int i = 0; i < hist.count; i++) {
        int idx = (hist.start + i) % hist.limit;
        printf("  %d %s\n", cmd_num_start + i, hist.entries[idx]);
    }
}

const char *hist_search_prefix(char *prefix) {
    if (!prefix) return NULL;
    int prefix_len = strlen(prefix);
    //int cmd_num_start = hist.next_cmd_num - hist.count;

    // Search backward through history (most recent first)
    for (int i = hist.count - 1; i >= 0; i--) {
        int idx = (hist.start + i) % hist.limit;
        if (strncmp(hist.entries[idx], prefix, prefix_len) == 0) {
            printf("checking hist entry by prefix: %s\n", hist.entries[idx]);
            return hist.entries[idx];
        }
    }
    return NULL;
}

const char *hist_search_cnum(int cnum) {
    for (int i = 0; i < hist.count; i++) {
        int idx = (hist.start + i) % hist.limit;
        if (hist.command_nums[idx] == cnum) {
            printf("checking hist entry by cnum: %s\n", hist.entries[idx]);
            return hist.entries[idx];  // ✅ full command string
        }
    }
    return NULL;
}

unsigned int hist_last_cnum(void) {
    return hist.next_cmd_num - 1;
}
