#include "kutil.h"

void llpush_front(linked_list_t *ll, linked_list_node_t *node) {
    node->prev = 0L;
    node->next = ll->head;
    if (ll->head == 0L) {
        ll->tail = node;
    } else {
        ll->head->prev = node;
    }
    ll->head = node;
}

void llpush_back(linked_list_t *ll, linked_list_node_t *node) {
    node->next = 0L;
    node->prev = ll->tail;
    if (ll->tail == 0L) {
        ll->head = node;
    } else {
        ll->tail->next = node;
    }
    ll->tail = node;
}

linked_list_node_t *llpop_front(linked_list_t *ll) {
    linked_list_node_t *n = ll->head;
    if (n) {
        ll->head = n->next;
        if (n->next == 0L) {
            ll->tail = 0L; // tail was head
        }
        n->next = 0L;
    }
    return n;
}

linked_list_node_t *llpop_back(linked_list_t *ll) {
    linked_list_node_t *n = ll->tail;
    if (n) {
        ll->tail = n->prev;
        if (n->prev == 0L) {
            ll->head = 0L; // head was tail
        }
        n->prev = 0L;
    }
    return n;
}

void *memset(void *s, int c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        *((unsigned char *)s + i) = c;
    }
    return s;
}
