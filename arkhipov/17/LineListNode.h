#ifndef LINE_LIST_NODE
#define LINE_LIST_NODE

typedef struct LLN {
    char* value;
    struct LLN* next;
} LineListNode;

int compare_str(char* str_first, char* str_second) {
    int sl_f = strlen(str_first);
    int sl_s = strlen(str_second);

    int sl_m = sl_f;
    if (sl_m > sl_s) {
        sl_m = sl_s;
    }

    for (int i = 0; i < sl_m; ++i) {
        if (str_first[i] < str_second[i]) {
            return 1;
        }
        if (str_second[i] < str_first[i]) {
            return -1;
        }
    }
    if (sl_f < sl_s) {
        return 1;
    }
    if (sl_s < sl_f) {
        return -1;
    }
    return 0;
}

void swap_with_next(LineListNode* node) {
    if (node == NULL) return;
    LineListNode* next = node->next;
    if (next == NULL) return;

    char* tmp = next->value;
    next->value = node->value;
    node->value = tmp;
}

void sort_list(LineListNode* node) {
    // 0 nodes case
    if (node == NULL) {
        return;
    }

    LineListNode* end = NULL;
    while (end != node) {
        LineListNode* cursor = node;
        while (cursor->next != end) {
            if (compare_str(cursor->value, cursor->next->value) == -1) {
                swap_with_next(cursor);
            }
            cursor = cursor->next;
        }
        end = cursor;
    }
}

void delete_list(LineListNode* cursor) {
    while (cursor != NULL) {
        LineListNode* next = cursor->next;
        free(cursor);
        cursor = next;
    }
}

void print_list(LineListNode* cursor) {
    while (cursor != NULL) {
        printf("%s\n", cursor->value);
        cursor = cursor->next;
    }
    printf("\n");
}

#endif
