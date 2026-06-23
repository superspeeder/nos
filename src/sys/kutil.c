#include "kutil.h"

void llpush_front(linked_list_t *ll, linked_list_node_t *node) {
    node->prev = nullptr;
    node->next = ll->head;
    if (ll->head == nullptr) {
        ll->tail = node;
    } else {
        ll->head->prev = node;
    }
    ll->head = node;
}

void llpush_back(linked_list_t *ll, linked_list_node_t *node) {
    node->next = nullptr;
    node->prev = ll->tail;
    if (ll->tail == nullptr) {
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
        if (n->next == nullptr) {
            ll->tail = nullptr; // tail was head
        }
        n->next = nullptr;
    }
    return n;
}

linked_list_node_t *llpop_back(linked_list_t *ll) {
    linked_list_node_t *n = ll->tail;
    if (n) {
        ll->tail = n->prev;
        if (n->prev == nullptr) {
            ll->head = nullptr; // head was tail
        }
        n->prev = nullptr;
    }
    return n;
}

size_t llist_len(linked_list_t *ll) {}

void *memset(void *s, int c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        *((unsigned char *)s + i) = c;
    }
    return s;
}
