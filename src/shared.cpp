#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>

#define KB(x) (x * 1024L)
#define MB(x) (KB(x) * 1024L)
#define GB(x) (MB(x) * 1024L)

typedef uint32_t u32;
typedef uint8_t u8;
typedef int32_t s32;
typedef uintmax_t umax;
typedef float real32;
typedef double real64;

template <class T>
struct v2 {
    T x, y;

    real32 length() {
        return sqrtf(x * x + y * y);
    }

    v2<T> operator-(v2<T> rhs) {
        v2<T> rv;
        rv.x = this->x - rhs.x;
        rv.y = this->y - rhs.y;
        return rv;
    }

    v2<T> operator*(T scalar) {
        v2<T> rv;
        rv.x = this->x * scalar;
        rv.y = this->y * scalar;
        return rv;
    }
};

enum terrain_names {
    FOG = 0,
    GROUND,
    WATER,
    HILLS
};

enum class unit_names {
    NONE,
    SOLDIER
};

void sitrep(char *fmt, ...);
u32 time_get_now_in_ms();

struct memory_arena {
    u8 *base;
    u32 max, used;
    char *name;
};

u8 *memory_arena_use(memory_arena *mem, u32 amount) {
    u8 *rv = mem->base + mem->used;
    if (mem->used + amount > mem->max) {
        sitrep("TOO MUCH MEMORY USED FOR '%s'\n", mem->name);
        assert(mem->used + amount <= mem->max);
    }
    mem->used += amount;
    return rv;
}

memory_arena memory_arena_child(memory_arena *parent, u32 size, char *name) {
    memory_arena rv;
    rv.base = parent->base + parent->used;
    if (parent->used + size > parent->max) {
        sitrep("TOO MUCH MEMORY USED FROM '%s' WHEN CREATING CHILD '%s'\n", parent->name, name);
        assert(parent->used + size <= parent->max);
    }
    parent->used += size;
    rv.max = size;
    rv.used = 0;
    rv.name = name;
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

        return lesser;
    }

	void add_to_read_it(u32 amount) {
		assert(distance() >= amount);
		read_it += amount;
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

template<class T>
struct doubly_linked_list_node {
    struct doubly_linked_list_node *prev, *next;
    T payload;
};

template<class T>
struct doubly_linked_list {
    struct doubly_linked_list_node<T> *first;

    u32 length() {
        doubly_linked_list_node<T> *iter = first;
        
        u32 rv = 0;
        while (iter) {
            ++rv;
            iter = iter->next;
        }

        return rv;
    }

    void push_front(T item) {
        if (!first) {
            first = (doubly_linked_list_node<T> *)malloc(sizeof(*first));
            first->prev = first->next = NULL;
            first->payload = item;
        } else {
            first->prev = (doubly_linked_list_node<T> *)malloc(sizeof(*first->prev));
            first->prev->prev = NULL;
            first->prev->next = first;
            first->prev->payload = item;
        }
    }

    void push_back(T item) {
        if (!first) {
            first = (doubly_linked_list_node<T> *)malloc(sizeof(*first));
            first->prev = first->next = NULL;
            first->payload = item;
        } else {
            doubly_linked_list_node<T> *iter = first;

            while(iter->next) {
                iter = iter->next;
            }

            iter->next = (doubly_linked_list_node<T> *)malloc(sizeof(*iter->next));
            iter->next->next = NULL;
            iter->next->prev = iter;
            iter->next->payload = item;
        }
    }

    void pop_front() {
        if (!first) {
            return;
        }

        first = first->next;
        free(first->prev);
        first->prev = NULL;
    }

    void pop_back() {
        if (!first) {
            return;
        }
        doubly_linked_list_node<T> *prev = first, *iter = first->next;

        while (iter) {
            prev = iter;
            iter = iter->next;
        }

        prev->prev->next = NULL;
        free(prev);
        return;
    }

    T *get(u32 idx) {
        T *rv = NULL;
        doubly_linked_list_node<T> *iter = first;

        u32 cnt = 0;
        while (iter) {
            if (idx == cnt) {
                rv = &iter->payload;
                break;
            }
            iter = iter->next;
            ++cnt;
        }

        return rv;
    }

    void erase(u32 idx) {
        doubly_linked_list_node<T> *iter = first;

        u32 cnt = 0;
        while (iter) {
            if (idx == cnt) {
                if (iter->prev) {
                    iter->prev->next = iter->next;
                }
                if (iter->next) {
                    iter->next->prev = iter->prev;
                }
                free(iter);
                if (first == iter)
                    first = NULL;
                break;
            }
            iter = iter->next;
            ++cnt;
        }
    }

    void clear() {
        doubly_linked_list_node<T> *iter = first, *iter_next = first->next;

        first = NULL;
        while (iter) {
            iter_next = iter->next;
            free(iter);
            iter = iter_next;
        }
    }
};
