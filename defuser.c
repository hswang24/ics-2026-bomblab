#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/* Values follow the bomb's 32-bit unsigned arithmetic, including wraparound. */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t low = (uint32_t)challenge;
    uint32_t middle = (uint32_t)(challenge >> 16);
    uint32_t high = (uint32_t)(challenge >> 32);
    uint32_t step1 = (high + UINT32_C(2) * middle) ^ UINT32_C(0x3dab);
    uint32_t step0 = (UINT32_C(5) * low + high) ^ UINT32_C(0x33b0);
    uint32_t values[] = {
        (UINT32_C(5) * step0) ^ (step1 + UINT32_C(0xdeb0))
    };
    return write_answer(out, cap, values, 1);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    values[0] = UINT32_C(3) * (uint32_t)((challenge >> 24) & UINT64_C(63));
    for (unsigned i = 1; i < 6; ++i) {
        uint32_t part = (uint32_t)((challenge >> (6 + 4 * (i - 1))) & UINT64_C(15));
        values[i] = UINT32_C(5) * ((part + i) ^ values[i - 1]);
    }
    return write_answer(out, cap, values, 6);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t index = (uint32_t)challenge & UINT32_C(7);
    uint32_t x = (uint32_t)(challenge >> 3) ^ (uint32_t)(challenge >> 32);
    uint32_t result;
    /* The jump table maps indices to case_1,3,6,4,0,5,2,7. */
    switch (index) {
        case 0: result = (x ^ UINT32_C(0xec93)) + (x >> 2); break;
        case 1: result = (UINT32_C(4) * x) ^ UINT32_C(0x3ccf); break;
        case 2: result = (UINT32_C(4) * x) ^ UINT32_C(0x8641); break;
        case 3: result = UINT32_C(5) * x + UINT32_C(0xcb43); break;
        case 4: result = UINT32_C(5) * x + UINT32_C(0x36899); break;
        case 5: result = (x ^ UINT32_C(0x31ac)) + (x >> 1); break;
        case 6: result = UINT32_C(3) * x + UINT32_C(0x84e9); break;
        default: result = UINT32_C(5) * x + UINT32_C(0x740e); break;
    }
    uint32_t values[] = {index, result};
    return write_answer(out, cap, values, 2);
}

static uint32_t phase4_walk(uint32_t target, uint32_t low, uint32_t high, uint32_t salt) {
    uint32_t middle = low + ((high - low) >> 1);
    if (target == middle) return (middle + UINT32_C(0x33)) ^ salt;
    uint32_t child;
    uint32_t branch;
    if (target < middle) {
        child = phase4_walk(target, low, middle - 1, salt);
        branch = UINT32_C(3);
    } else {
        child = phase4_walk(target, middle + 1, high, salt);
        branch = UINT32_C(8);
    }
    return (UINT32_C(5) * child) ^ (middle + salt) ^ branch;
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t target = (uint32_t)challenge & UINT32_C(127);
    uint32_t salt = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);
    uint32_t values[] = {phase4_walk(target, 0, 127, salt)};
    return write_answer(out, cap, values, 1);
}

struct phase5_node {
    unsigned next;
    uint32_t value;
    uint32_t salt;
};

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    /* Nodes are numbered by their order in the bomb's p5_nodes array. */
    static const struct phase5_node nodes[12] = {
        {5,  UINT32_C(0x0b4a), UINT32_C(0xe99d)},
        {8,  UINT32_C(0x0178), UINT32_C(0x5372)},
        {10, UINT32_C(0x0a70), UINT32_C(0x8441)},
        {6,  UINT32_C(0x0976), UINT32_C(0x491f)},
        {11, UINT32_C(0x057d), UINT32_C(0x7225)},
        {9,  UINT32_C(0x047e), UINT32_C(0x4c05)},
        {12, UINT32_C(0x030a), UINT32_C(0x9d21)},
        {0,  UINT32_C(0x085e), UINT32_C(0x16af)},
        {3,  UINT32_C(0x0738), UINT32_C(0x6e76)},
        {1,  UINT32_C(0x0632), UINT32_C(0xf675)},
        {4,  UINT32_C(0x0024), UINT32_C(0x4a17)},
        {7,  UINT32_C(0x0210), UINT32_C(0x2fee)}
    };
    static const unsigned heads[8] = {2, 10, 4, 11, 7, 0, 5, 9};
    unsigned node = heads[challenge & UINT64_C(7)];
    unsigned hops = (unsigned)((challenge >> 3) & UINT64_C(3));
    uint32_t total = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32)
                     ^ UINT32_C(0xaeba);
    uint32_t last = 0;
    for (unsigned step = 0; step <= hops; ++step) {
        total = (total ^ nodes[node].salt) + nodes[node].value;
        last = nodes[node].value;
        if (step < hops) node = nodes[node].next;
    }
    uint32_t values[] = {total, last};
    return write_answer(out, cap, values, 2);
}

static uint32_t rotate_right(uint32_t value, unsigned shift) {
    return (value >> shift) | (value << (32 - shift));
}

static uint32_t rotate_left(uint32_t value, unsigned shift) {
    return (value << shift) | (value >> (32 - shift));
}

static uint32_t phase6_op(unsigned index, uint32_t x) {
    /* p6_ops is {op_0, op_5, op_4, op_3, op_2, op_0, op_3, op_1}. */
    switch (index) {
        case 0:
        case 5: return (x ^ UINT32_C(0xa6ab)) + UINT32_C(2) * x;
        case 1: return rotate_left(x, 3) ^ UINT32_C(0xba54);
        case 2: return (x + UINT32_C(0xaf58)) ^ (x >> 2);
        case 3:
        case 6: return ((x >> 1) ^ x) + UINT32_C(0x13a2);
        case 4: return rotate_right(x, 5);
        default: return ((x << 7) + UINT32_C(0xac0d)) ^ (UINT32_C(3) * x);
    }
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    unsigned first = (unsigned)(challenge & UINT64_C(7));
    unsigned second = (unsigned)((challenge >> 3) & UINT64_C(7));
    uint32_t x = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);
    uint32_t intermediate = phase6_op(first, x) ^ UINT32_C(0x2485);
    uint32_t values[] = {phase6_op(second, intermediate) + UINT32_C(0x259b)};
    return write_answer(out, cap, values, 1);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    uint32_t selector = (uint32_t)context & UINT32_C(3);
    uint32_t a = (uint32_t)challenge ^ (uint32_t)(challenge >> 32);
    uint32_t b = (uint32_t)context ^ (uint32_t)(context >> 32);
    uint32_t second = rotate_right(a ^ b, 3) ^ UINT32_C(0x9f8a)
                      ^ (UINT32_C(0xfd07) * selector) ^ (uint32_t)(context >> 16);
    uint32_t third = second ^ (b + selector + UINT32_C(0x628d) + (a >> 3));
    /* secret_copy starts at rbp-0xd0: OPEN belongs at offset 32,
       and the numeric tail starts at offset 36. No return address is touched. */
    int size = snprintf(out, cap, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAOPEN%" PRIu32
                        " %" PRIu32 " %" PRIu32, selector, second, third);
    return size < 0 || (size_t)size >= cap;
}
