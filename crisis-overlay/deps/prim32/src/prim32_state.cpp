// ============================================================================
// Prim32 UI-state store — open-addressing hash map with a separate dense
// entry array. slots[hash(id) % slotCap] stores entry_index + 1 (0 = empty).
// The dense layout keeps iteration for pruning cache-friendly; the slot table
// is fully rebuilt after any compaction rather than using tombstones.
// ============================================================================
#include "prim32_state.h"
#include <stdlib.h>
#include <string.h>

namespace prim32 {

static inline uint32_t hash_id( uint32_t id ) {
    return id * 2654435769u;
}

static uint32_t next_pow2( uint32_t v ) {
    if ( v < 8 ) return 8;
    --v;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

static void rebuild_slots( Prim32StateStore* store ) {
    memset( store->slots, 0, store->slotCapacity * sizeof( uint32_t ) );
    for ( uint32_t i = 0; i < store->count; ++i ) {
        uint32_t h = hash_id( store->entries[ i ].id ) % store->slotCapacity;
        while ( store->slots[ h ] )
            h = ( h + 1 ) % store->slotCapacity;
        store->slots[ h ] = i + 1;
    }
}

bool Prim32StateStoreInit( Prim32StateStore* store, uint32_t initialCapacity ) {
    if ( !initialCapacity ) initialCapacity = 8;
    uint32_t slotCap = next_pow2( initialCapacity * 2 );
    store->entries = static_cast< Prim32StateEntry* >(
        malloc( initialCapacity * sizeof( Prim32StateEntry ) ) );
    store->slots   = static_cast< uint32_t* >(
        calloc( slotCap, sizeof( uint32_t ) ) );
    if ( !store->entries || !store->slots ) {
        free( store->entries ); store->entries = nullptr;
        free( store->slots );   store->slots   = nullptr;
        store->count = store->capacity = store->slotCapacity = 0;
        return false;
    }
    store->count        = 0;
    store->capacity     = initialCapacity;
    store->slotCapacity = slotCap;
    return true;
}

void Prim32StateStoreFree( Prim32StateStore* store ) {
    free( store->entries ); store->entries = nullptr;
    free( store->slots );   store->slots   = nullptr;
    store->count = store->capacity = store->slotCapacity = 0;
}

Prim32StateEntry* Prim32StateStoreFind( Prim32StateStore* store, uint32_t id ) {
    if ( !store->slotCapacity ) return nullptr;
    uint32_t h = hash_id( id ) % store->slotCapacity;
    for ( uint32_t probe = 0; probe < store->slotCapacity; ++probe ) {
        uint32_t s = ( h + probe ) % store->slotCapacity;
        if ( !store->slots[ s ] ) return nullptr;
        uint32_t idx = store->slots[ s ] - 1;
        if ( store->entries[ idx ].id == id ) return &store->entries[ idx ];
    }
    return nullptr;
}

Prim32StateEntry* Prim32StateStoreFindOrCreate( Prim32StateStore* store, uint32_t id, uint32_t frame ) {
    // Grow when load factor hits 50% to keep linear-probe chains short.
    if ( store->count * 2 >= store->slotCapacity ) {
        uint32_t newCap     = store->capacity ? store->capacity * 2 : 8;
        uint32_t newSlotCap = next_pow2( newCap * 2 );
        auto* newEntries = static_cast< Prim32StateEntry* >(
            malloc( newCap * sizeof( Prim32StateEntry ) ) );
        auto* newSlots = static_cast< uint32_t* >(
            calloc( newSlotCap, sizeof( uint32_t ) ) );
        if ( !newEntries || !newSlots ) {
            free( newEntries );
            free( newSlots );
            return nullptr;
        }
        if ( store->entries )
            memcpy( newEntries, store->entries, store->count * sizeof( Prim32StateEntry ) );
        free( store->entries );
        free( store->slots );
        store->entries      = newEntries;
        store->capacity     = newCap;
        store->slots        = newSlots;
        store->slotCapacity = newSlotCap;
        rebuild_slots( store );
    }

    uint32_t h = hash_id( id ) % store->slotCapacity;
    for ( uint32_t probe = 0; probe < store->slotCapacity; ++probe ) {
        uint32_t s = ( h + probe ) % store->slotCapacity;
        if ( !store->slots[ s ] ) {
            Prim32StateEntry* e = &store->entries[ store->count ];
            memset( e, 0, sizeof( *e ) );
            e->id        = id;
            e->lastFrame = frame;
            store->slots[ s ] = ++store->count;
            return e;
        }
        uint32_t idx = store->slots[ s ] - 1;
        if ( store->entries[ idx ].id == id ) {
            store->entries[ idx ].lastFrame = frame;
            return &store->entries[ idx ];
        }
    }
    return nullptr;
}

uint32_t Prim32StateStorePrune( Prim32StateStore* store, uint32_t minLastFrame ) {
    uint32_t removed = 0, w = 0;
    for ( uint32_t r = 0; r < store->count; ++r ) {
        if ( store->entries[ r ].lastFrame >= minLastFrame )
            store->entries[ w++ ] = store->entries[ r ];
        else
            ++removed;
    }
    if ( removed ) {
        store->count = w;
        rebuild_slots( store );
    }
    return removed;
}

} // namespace prim32
