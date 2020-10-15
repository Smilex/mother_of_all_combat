#include <stdint.h>
#include <assert.h>

#define KB(x) (x * 1024)
#define MB(x) (KB(x) * 1024)

typedef uint32_t u32;
typedef uint8_t u8;
typedef int32_t s32;
typedef uintmax_t umax;

enum terrain_names {
    FOG = 0,
    GROUND,
    WATER,
    HILLS
};

struct memory_arena {
    u8 *base;
    u32 max, used;
};

u8 *memory_arena_use(memory_arena *mem, u32 amount) {
    u8 *rv = mem->base + mem->used;
    assert(mem->used + amount <= mem->max);
    mem->used += amount;
    return rv;
}

memory_arena memory_arena_child(memory_arena *parent, u32 size) {
    memory_arena rv;
    rv.base = parent->base + parent->used;
    assert(parent->used + size <= parent->max);
    parent->used += size;
    rv.max = size;
    rv.used = 0;
    return rv;
}
