#ifndef HI_ENTITY_H
#define HI_ENTITY_H

// Every dynamic object in your game is an "entity". Entities are updated and
// drawn once per frame. You can overwrite various default functions of entity
// with your own, entity specific, implementation.

// !! Before you include this header, you have to define your entity_t struct,
// (through the ENTITY_DEFINE() macro), your entity_message_t enum and the 
// ENTITY_TYPES() X-Macro with all your entity types.

// See entity_def.h for the basic struct that is used by ENTITY_DEFINE()

#include "types.h"
#include "trace.h"
#include "entity_def.h"
#include "../libs/pl_json.h"


// Trigger compilation errors if any of the required #defines or typedefs are
// not defined.

#ifndef ENTITY_TYPES
	#error "#define the X-macro ENTITY_TYPES() before including entity.h"
#endif

struct _entity_assert_types_defined {
	entity_t CALLL_ENTITY__DEFINE_BEFORE__INCLUDING_ENTITY_H;
	entity_message_t DEFINE_ENUM__entity_message_t__BEFORE_INCLUDING_ENTITY_H;
};



// The maximum amount of entities that are in your game at once. Beyond that,
// entity_spawn() will return NULL.
#if !defined(ENTITIES_MAX)
	#define ENTITIES_MAX 1024
#endif

// The maximum size any of your entities is expected to have. This only affects
// the accuracy of entities_by_proximity() and entities_by_location().
// FIXME: this is bad; we should have to specify this.
#if !defined(ENTITY_MAX_SIZE)
	#define ENTITY_MAX_SIZE 64.0
#endif

// The minimum velocity of an entities (that has restitution > 0) for it to
// bounce. If this would be 0.0, entities would bounce indefinitely with ever
// smaller velocities.
#if !defined(ENTITY_MIN_BOUNCE_VELOCITY)
	#define ENTITY_MIN_BOUNCE_VELOCITY 10.0
#endif

// The axis (x or y) on which we want to do the broad phase collision detection
// sweep & prune. For mosly horizontal games it should be x, for vertical ones y
#if !defined(ENTITY_SWEEP_AXIS)
	#define ENTITY_SWEEP_AXIS x
#endif

// The entity_vtab_t struct must implemented by all your entity types. It holds
// the functions to call for each entity type. All of these are optional. In
// the simplest case you just have a global:
// entity_vtab_t entity_vtab_mytype = {};
typedef struct {
	// Called once at program start, just before main_init(). Use this to
	// load assets and animations for your entity types.
	void (*load)(void);

	// Called once for each entity, when the entity is created through
	// entity_spawn(). Use this to set all properties (size, offset, animation)
	// of your entity.
	void (*init)(entity_t *self);

	// Called once after engine_load_level() when all entities have been 
	// spawned. The json_t *def contains the "settings" of the entity from the
	// level json.
	void (*settings)(entity_t *self, json_t *def);

	// Called once per frame for each entity. The default entity_update_base()
	// moves the entity according to its physics
	void (*update)(entity_t *self);

	// Called once per frame for each entity. The default entity_draw_base()
	// draws the entity->anim 
	void (*draw)(entity_t *self, vec2_t viewport);

	// Called when the entity is removed from the game through entity_kill()
	void (*kill)(entity_t *self);

	// Called when this entity touches another entity, according to 
	// entity->check_against
	void (*touch)(entity_t *self, entity_t *other);

	// Called when the entity collides with the game world or another entity
	// Careful: the trace will only be set from a game world collision. It will
	// be NULL for a collision with another entity.
	void (*collide)(entity_t *self, vec2_t normal, trace_t *trace);

	// Called through entity_damage(). The default entity_base_damage() deducts
	// damage from the entity's health and calls entity_kill() if it's <= 0.
	void (*damage)(entity_t *self, entity_t *other, float damage);

	// Called through entity_trigger()
	void (*trigger)(entity_t *self, entity_t *other);

	// Called through entity_message()
	void (*message)(entity_t *self, entity_message_t message, void *data);
} entity_vtab_t;


// Macros to call the correct function on each entity according to the vtab.
extern entity_vtab_t entity_vtab[ENTITY_TYPES_COUNT];
#define entity_is_type(SELF, TYPE)            (SELF && SELF->type == TYPE)
#define entity_init(ENTITY)                   entity_vtab[ENTITY->type].init(ENTITY)
#define entity_settings(ENTITY, DESC)         entity_vtab[ENTITY->type].settings(ENTITY, DESC)
#define entity_update(ENTITY)                 entity_vtab[ENTITY->type].update(ENTITY)
#define entity_draw(ENTITY, VIEWPORT)         entity_vtab[ENTITY->type].draw(ENTITY, VIEWPORT)
#define entity_kill(ENTITY)                   (ENTITY->is_alive = false, entity_vtab[ENTITY->type].kill(ENTITY))
#define entity_touch(ENTITY, OTHER)           entity_vtab[ENTITY->type].touch(ENTITY, OTHER)
#define entity_collide(ENTITY, NORMAL, TRACE) entity_vtab[ENTITY->type].collide(ENTITY, NORMAL, TRACE)
#define entity_damage(ENTITY, OTHER, DAMAGE)  entity_vtab[ENTITY->type].damage(ENTITY, OTHER, DAMAGE)
#define entity_trigger(ENTITY, OTHER)         entity_vtab[ENTITY->type].trigger(ENTITY, OTHER)
#define entity_message(ENTITY, MESSAGE, DATA) entity_vtab[ENTITY->type].message(ENTITY, MESSAGE, DATA)

// Return a reference for to given entity
entity_ref_t entity_ref(entity_t *self);

// Get an entity by its reference. This will be NULL if the referred entity is
// not valid anymore.
entity_t *entity_by_ref(entity_ref_t ref);

// Spawn entity of the given type at the given position, returns NULL if entity
// storage is full.
entity_t *entity_spawn(entity_type_t type, vec2_t pos);

// Get the type enum by its type name
entity_type_t entity_type_by_name(char *type_name);

// Get the type name by its type enum
const char *entity_type_name(entity_type_t type);

// The center position of an entity, according to its pos and size
vec2_t entity_center(entity_t *ent);

// The distance (in pixels) between two entities
float entity_dist(entity_t *a, entity_t *b);

// The angle in radians of a line between two entities
float entity_angle(entity_t *a, entity_t *b); 

// Updates the entity's position and velocity according to its physics. This
// also checks for collision against the game world. If you use update() in your
// vtab, you may still want to call this function.
void entity_base_update(entity_t *self);

// Draws the entity->anim. If you use draw() in you vtab, you may still want to
// call this function.
void entity_base_draw(entity_t *self, vec2_t viewport);

// Deduct damage from health; calls entity_kill() when health <= 0. If you use
// damage() in your vtab, you may still want to call this function.
void entity_base_damage(entity_t *self, entity_t *other, float damage);

// Get the name of an entity (usually the name is specified through "settings"
// in a level json). May be NULL.
entity_t *entity_by_name(char *name);

// Get a list of entities that are within the radius of this entity. Optionally
// filter by one entity type. Use ENTITY_TYPE_NONE to get all entities in
// proximity.
// If called while the game is running (as opposed to during scene init), the 
// list is only valid for the duration of the current frame.
entity_list_t entities_by_proximity(entity_t *ent, float radius, entity_type_t type);

// Same as entities_by_proximity() but with a center position instead of an
// entity.
// If called while the game is running (as opposed to during scene init), the 
// list is only valid for the duration of the current frame.
entity_list_t entities_by_location(vec2_t pos, float radius, entity_type_t type, entity_t *exclude);

// Get a list of all entities of a certain type
// If called while the game is running (as opposed to during scene init), the 
// list is only valid for the duration of the current frame.
entity_list_t entities_by_type(entity_type_t type);

// Get a list of entities by name, with json_t array or object of names.
// If called while the game is running (as opposed to during scene init), the 
// list is only valid for the duration of the current frame.
entity_list_t entities_from_json_names(json_t *targets);

// Whether two entities are overlapping
bool entity_is_touching(entity_t *self, entity_t *other);


// These functions are called by the engine during scene init/update/cleanup
void entities_init(void);
void entities_cleanup(void);
void entities_reset(void);
void entities_update(void);
void entities_draw(vec2_t viewport);

#endif