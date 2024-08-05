#ifndef HI_ENTITY_DEF_H
#define HI_ENTITY_DEF_H


// Entity refs can be used to safely keep track of entities. Refs can be
// resolved to an actual entity_t with entity_by_ref(). Refs will resolve to
// NULL, if the referenced entity is no longer valid (i.e. dead). This prevents
// errors with direct entity_t* which will always point to a valid entity 
// storage, but may no longer be the entity that you wanted.
typedef struct {
	uint16_t id;
	uint16_t index;
} entity_ref_t;

// A list of entity refs. Usually bump allocated and thus only valid for the
// current frame.
typedef struct {
	uint32_t len;
	entity_ref_t *entities;
} entity_list_t;

// entity_ref_none will always resolve to NULL
#define entity_ref_none() (entity_ref_t){.id = 0, .index = 0}

// Entities can be members of one or more groups (through ent->group). This can
// be used in conjunction with ent->check_against to indicate for which pairs
// of entities you want to get notified by entity_touch(). Groups can be or-ed 
// together.
// E.g. with the following two entities
//   ent_a->group = ENTITY_GROUP_ITEM | ENTITY_GROUP_BREAKABLE;
//   ent_b->check_against = ENTITY_GROUP_BREAKABLE;
// The function 
//   entity_touch(ent_b, ent_a) 
// will be called when those two entities overlap.
typedef enum {
	ENTITY_GROUP_NONE =       (0),
	ENTITY_GROUP_PLAYER =     (1 << 0),
	ENTITY_GROUP_NPC =        (1 << 1),
	ENTITY_GROUP_ENEMY =      (1 << 2),
	ENTITY_GROUP_ITEM =       (1 << 3),
	ENTITY_GROUP_PROJECTILE = (1 << 4),
	ENTITY_GROUP_PICKUP =     (1 << 5),
	ENTITY_GROUP_BREAKABLE =  (1 << 6),
} entity_group_t;


// The collision modes used by entity_physics_t. You may use these directly if
// entity_physics_t doesn't have a configuration that you need. E.g. if you want
// an entity to collide with other entities, but NOT with the collision_map, you
// could extend entity_physics_t with 
//   ENTITY_PHYSICS_MOVE | ENTITY_COLLIDES_ACTIVE
typedef enum {
	ENTITY_COLLIDES_WORLD   = (1 << 1),
	ENTITY_COLLIDES_LITE    = (1 << 4),
	ENTITY_COLLIDES_PASSIVE = (1 << 5),
	ENTITY_COLLIDES_ACTIVE  = (1 << 6),
	ENTITY_COLLIDES_FIXED   = (1 << 7),
} entity_collision_mode_t;

// The ent->physics determines how and if entities are moved and collide.
typedef enum {
	// Don't collide, don't move. Useful for items that just sit there.
	ENTITY_PHYSICS_NONE    = 0,

	// Move the entity according to its velocity, but don't collide
	ENTITY_PHYSICS_MOVE    = (1 << 0),

	// Move the entity and collide with the collision_map
	ENTITY_PHYSICS_WORLD   = ENTITY_PHYSICS_MOVE  | ENTITY_COLLIDES_WORLD,

	// Move the entity, collide with the collision_map and other entities, but
	// only those other entities that have matching physics:
	// In ACTIVE vs. LITE or FIXED vs. ANY collisions, only the "weak" entity 
	// moves, while the other one stays fixed. In ACTIVE vs. ACTIVE and ACTIVE 
	// vs. PASSIVE collisions, both entities are moved. LITE or PASSIVE entities
	// don't collide with other LITE or PASSIVE entities at all. The behaiviour
	// for FIXED vs. FIXED collisions is undefined.
	ENTITY_PHYSICS_LITE    = ENTITY_PHYSICS_WORLD | ENTITY_COLLIDES_LITE,
	ENTITY_PHYSICS_PASSIVE = ENTITY_PHYSICS_WORLD | ENTITY_COLLIDES_PASSIVE,
	ENTITY_PHYSICS_ACTIVE  = ENTITY_PHYSICS_WORLD | ENTITY_COLLIDES_ACTIVE,
	ENTITY_PHYSICS_FIXED   = ENTITY_PHYSICS_WORLD | ENTITY_COLLIDES_FIXED,
} entity_physics_t;

typedef struct entity_t entity_t;

// ENTITY_DEFINE(), called from your code BEFORE including entity.h but after
// entity_def.h, will define the entity_struct. This will also create the 
// ENTITY_TYPE_* enum according to the ENTITY_TYPES() x-macro in your code.
// E.g.:
/*
     #include "entity_def.h"
     ENTITY_TYPES(TYPE) \  
        TYPE(ENTITY_TYPE_PLAYER, player) \  
        TYPE(ENTITY_TYPE_ENEMY, enemy)  
     ENTITY_DEFINE()
     #include "entity.n"
*/
#define ENTITY_DECLARE_TYPE_ENUM(ENUM, NAME) ENUM,
#define ENTITY_DEFINE(...) \
	typedef enum { \
		ENTITY_TYPE_NONE = 0, \
		ENTITY_TYPES(ENTITY_DECLARE_TYPE_ENUM) \
		ENTITY_TYPES_COUNT \
	} entity_type_t; \
	\
	struct entity_t { \
		uint16_t id; /* A unique id for this entity, assigned on spawn */ \
		bool is_alive; /* Determines if this entity is in use */ \
		bool on_ground; /* True for engine.gravity > 0 and standing on something */ \
		int32_t draw_order; /* Entities are sorted (ascending) by this before drawing */ \
		entity_type_t type; /* The ENTITY_TYPE_* */ \
		entity_physics_t physics; /* Physics behavior */ \
		entity_group_t group; /* The groups this entity belongs to */ \
		entity_group_t check_against; /* The groups that this entity can touch */ \
		vec2_t pos; /* Top left position of the bounding box in the game world; usually not manipulated directly */ \
		vec2_t size; /* The bounding box for physics */ \
		vec2_t vel; /* Velocity */ \
		vec2_t accel; /* Acceleration */ \
		vec2_t friction; /* Friction as a factor of engine.tick * velocity */ \
		vec2_t offset; /* Offset from position to draw the anim */ \
		char *name; /* Name used for targets etc. usually set through json data */ \
		float health; /* When entity is damaged an resulting health < 0, the entity is killed */ \
		float gravity; /* Gravity factor with engine.gravity. Default 1.0 */ \
		float mass; /* Mass factor for active collisions. Default 1.0 */\
		float restitution; /* The "bounciness factor" */ \
		float max_ground_normal; /* For slopes, determines how steep the slope can be to set on_ground flag. Default cosf(to_radians(46)) */ \
		float min_slide_normal; /* For slopes, determines how steep the slope has to be for entity to slide down. Default cosf(to_radians(0)) */ \
		anim_t anim; /* The animation that is automatically drawn */ \
		__VA_ARGS__ /* Your own properties, defined through ENTITY_DEFINE(...) */ \
	};

// These macros can be used in your entities/*.c files to configure their
// appearance in the enditor (tools/weltmeister.html)
#define EDITOR_SIZE(X, Y)     // Size in the editor. Default (8, 8)
#define EDITOR_RESIZE(RESIZE) // Whether the entity is resizable in the editor. Default false
#define EDITOR_COLOR(R, G, B) // The box color in the editor. Default (128, 255, 128)
#define EDITOR_IGNORE(IGNORE) // Whether this entity can be created in the editor. Default false

#endif
