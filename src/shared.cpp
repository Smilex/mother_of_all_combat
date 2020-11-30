#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>

#define KB(x) (x * 1024L)
#define MB(x) (KB(x) * 1024L)
#define GB(x) (MB(x) * 1024L)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

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

    bool operator==(v2<T> rhs) {
        return (this->x == rhs.x && this->y == rhs.y);
    }

    bool operator!=(v2<T> rhs) {
        return (this->x != rhs.x || this->y != rhs.y);
    }
};

enum terrain_names {
    FOG = 0,
    GRASS,
    WATER,
    DESERT
};

enum class entity_types {
    STRUCTURE,
    UNIT
};

enum class unit_names {
    NONE,
    SOLDIER,
    CARAVAN
};

enum sitrep_names {
    SITREP_INFO,
    SITREP_WARNING,
    SITREP_ERROR,
    SITREP_DEBUG
};

void sitrep(enum sitrep_names name, char *fmt, ...);
u32 time_get_now_in_ms();

struct memory_arena {
    u8 *base;
    u32 max, used;
    char *name;
};

u8 *memory_arena_use(memory_arena *mem, u32 amount) {
    u8 *rv = mem->base + mem->used;
    if (mem->used + amount > mem->max) {
        sitrep(SITREP_ERROR, "TOO MUCH MEMORY USED FOR '%s'", mem->name);
        assert(mem->used + amount <= mem->max);
    }
    mem->used += amount;
    return rv;
}

memory_arena memory_arena_child(memory_arena *parent, u32 size, char *name) {
    memory_arena rv;
    rv.base = parent->base + parent->used;
    if (parent->used + size > parent->max) {
        sitrep(SITREP_ERROR, "TOO MUCH MEMORY USED FROM '%s' WHEN CREATING CHILD '%s'", parent->name, name);
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
    
        return amount;
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
            first = first->prev;
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
        if (!first) return;
        doubly_linked_list_node<T> *iter = first, *iter_next;

        first = NULL;
        while (iter) {
            iter_next = iter->next;
            free(iter);
            iter = iter_next;
        }
    }
};

template<class T>
struct priority_queue_node {
    priority_queue_node<T> *next;
    u32 priority;
    T payload;
};

template<class T>
struct priority_queue {
    priority_queue_node<T> *first;

    ~priority_queue() {
        if (!first) return;
        priority_queue_node<T> *it = first, *it_next;

        while (it) {
            it_next = it->next;
            free(it);
            it = it_next;
        }
    }

    void push(T payload, u32 priority) {
        if (!first) {
            first = (priority_queue_node<T> *)malloc(sizeof(*first));
            first->next = NULL;
            first->priority = priority;
            first->payload = payload;
        } else {
            if (priority < first->priority) {
                priority_queue_node<T> *n = (priority_queue_node<T> *)malloc(sizeof(*n));
                n->next = first;
                n->priority = priority;
                n->payload = payload;
                first = n;
            } else {
                priority_queue_node<T> *it = first->next, *prev = first;

                while (it && it->priority < priority) {
                    prev = it;
                    it = it->next;
                }

                if (!it) {
                    it = (priority_queue_node<T> *)malloc(sizeof(*it));
                    it->next = NULL;
                    it->priority = priority;
                    it->payload = payload;
                    prev->next = it;
                } else {
                    prev->next = (priority_queue_node<T> *)malloc(sizeof(*it));
                    prev->next->next = it;
                    prev->next->priority = priority;
                    prev->next->payload = payload;
                }
            }
        }
    }

    T pop() {
        priority_queue_node<T> *next = first->next;
        T rv = first->payload;
        free(first);
        first = next;
        return rv;
    }

    bool empty() {
        return (first == NULL);
    }
};

template<class T, class K>
struct dictionary_node {
    dictionary_node<T, K> *next;
    T key;
    K payload;
};

template<class T, class K>
struct dictionary {
    dictionary_node<T, K> *first;

    ~dictionary() {
        if (!first) return;
    
        dictionary_node<T, K> *it = first, *it_next;
        while (it) {
            it_next = it->next;
            free(it);
            it = it_next;
        }
    }

    void push(T key, K payload) {
        if (!first) {
            first = (dictionary_node<T, K> *)malloc(sizeof(*first));
            first->next = NULL;
            first->key = key;
            first->payload = payload;
        } else {
            dictionary_node<T, K> *n = (dictionary_node<T, K> *)malloc(sizeof(*n));
            n->next = first;
            n->key = key;
            n->payload = payload;
            first = n;
        }
    }

    K *get(T key) {
        dictionary_node<T, K> *it = first;

        while (it) {
            if (it->key == key) {
                return &it->payload;
            }

            it = it->next;
        }

        return NULL;
    }
};

struct server_input {
    bool start_game;
};

struct server_output {
    u32 current_turn_id;
};

struct entity {
    entity_types type;
    v2<u32> position;
    s32 owner;
    u32 server_id;
};

struct structure : public entity {
    unit_names construction;
    u32 construction_timer;
};

struct unit : public entity {
    unit_names name;
    u32 action_points;
    unit *slot, *loaded_by;
};

entity *
find_entity_by_server_id(doubly_linked_list<entity *> entities, u32 id) {
    auto iter = entities.first;

    while (iter) {
        auto ent = iter->payload;
        if (ent->server_id == id)
            return ent;

        iter = iter->next;
    }

    return NULL;
}

entity **
find_entities_at_position(doubly_linked_list<entity *> entities, v2<u32> pos, memory_arena *mem, u32 *num_entities) {
    entity **rv = (entity **)(mem->base + mem->used);
    auto iter = entities.first;

    *num_entities = 0;
    while (iter) {
        auto ent = iter->payload;
        if (ent->position == pos) {
            memory_arena_use(mem, sizeof(*rv));
            rv[*num_entities] = ent;
            *num_entities = (*num_entities) + 1;
        }
        iter = iter->next;
    }

    return rv;
}
