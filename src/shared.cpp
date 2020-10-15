#include <stdint.h>
#include <assert.h>

#define KB(x) (x * 1024)
#define MB(x) (KB(x) * 1024)

typedef uint32_t u32;
typedef uint8_t u8;
typedef int32_t s32;
typedef uintmax_t umax;
typedef float real32;

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

template <class T>
struct ring_buffer {
    T *base;
    u32 max, read_it, write_it;

    ring_buffer(memory_arena *mem, u32 size) {
        this->base = (T *)memory_arena_use(mem, size * sizeof(T));
        this->max = size;
        this->read_it = this->write_it = 0;
    }

    u32 write(T *data, u32 amount) {
        u32 dist = distance();
        assert(dist + amount <= max);

        for (u32 i = 0; i < amount; ++i) {
            if (write_it == max) {
                write_it = 0;
            }

            base[write_it] = data[i];
            ++write_it;
        }
    }

    u32 read(T *buffer, u32 size) {
        u32 dist = distance();
        u32 lesser = dist;
        if (size < lesser)
            lesser = size;

        for (u32 i = 0; i < lesser; ++i) {
            if (read_it == max) {
                read_it = 0;
            }

            buffer[i] = base[read_it];
            ++read_it;
        }
    }

    u32 distance() {
        u32 rv = 0;
        if (write_it < read_it) {
            rv += max - read_it;
            rv += read_it - write_it;
        } else {
            rv += write_it - read_it;
        }

        return rv;
    }
};
