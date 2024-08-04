#include <string.h>
#include "entity.h"
#include "utils.h"
#include "engine.h"
#include "alloc.h"
#include "trace.h"
#include "platform.h"


#define ENTITY_STRINGIFY_NAME(ENUM, NAME) [ENUM] = #NAME,
static const char *entity_type_names[] = {
	ENTITY_TYPES(ENTITY_STRINGIFY_NAME)
};

entity_vtab_t entity_vtab[ENTITY_TYPES_COUNT];

static uint32_t entities_len = 0;
static uint16_t entity_unique_id = 0;
static entity_t *entities[ENTITIES_MAX];
static entity_t entities_storage[ENTITIES_MAX];

static void entity_move(entity_t *self, vec2_t vstep);
static void entity_handle_trace_result(entity_t *self, trace_t *t);
static void entity_resolve_collision(entity_t *a, entity_t *b);
static void entities_separate_on_x_axis(entity_t *left, entity_t *right, float left_move, float right_move, float overlap);
static void entities_separate_on_y_axis(entity_t *top, entity_t *bottom, float top_move, float bottom_move, float overlap);


static void noop_load(void) {}
static void noop_init(entity_t *self) {}
static void noop_kill(entity_t *self) {}
static void noop_settings(entity_t *self, json_t *def) {}
static void noop_touch(entity_t *self, entity_t *other) {}
static void noop_collide(entity_t *self, vec2_t normal, trace_t *trace) {}
static void noop_trigger(entity_t *self, entity_t *other) {}
static void noop_message(entity_t *self, entity_message_t message, void *data) {}

void entities_init(void) {
	// Set up the vtab for all entity types and provide default implementations
	// for functions that are not overridden. Some of the defaults are a simple
	// no-op. This is a tiny bit faster than checking if the function pointer
	// is NULL each time before invocation.
	#define ENTITY_INIT_VTAB(ENUM, NAME) \
		extern entity_vtab_t entity_vtab_##NAME; \
		entity_vtab[ENUM] = entity_vtab_##NAME;
	ENTITY_TYPES(ENTITY_INIT_VTAB)

	for (uint32_t i = 0; i < ENTITY_TYPES_COUNT; i++) {
		if (entity_vtab[i].load == NULL)     { entity_vtab[i].load = noop_load; }
		if (entity_vtab[i].init == NULL)     { entity_vtab[i].init = noop_init; }
		if (entity_vtab[i].settings == NULL) { entity_vtab[i].settings = noop_settings; }
		if (entity_vtab[i].update == NULL)   { entity_vtab[i].update = entity_base_update; }
		if (entity_vtab[i].draw == NULL)     { entity_vtab[i].draw = entity_base_draw; }
		if (entity_vtab[i].kill == NULL)     { entity_vtab[i].kill = noop_kill; }
		if (entity_vtab[i].touch == NULL)    { entity_vtab[i].touch = noop_touch; }
		if (entity_vtab[i].collide == NULL)  { entity_vtab[i].collide = noop_collide; }
		if (entity_vtab[i].damage == NULL)   { entity_vtab[i].damage = entity_base_damage; }
		if (entity_vtab[i].trigger == NULL)  { entity_vtab[i].trigger = noop_trigger; }
		if (entity_vtab[i].message == NULL)  { entity_vtab[i].message = noop_message; }
	}

	// Call load function on all entity types
	for (uint32_t ti = 0; ti < ENTITY_TYPES_COUNT; ti++) {
		entity_vtab[ti].load();
	}

	entities_reset();
}

void entities_cleanup(void) {
	entities_reset();
}

void entities_reset(void) {
	for (int i = 0; i < ENTITIES_MAX; i++) {
		entities[i] = &entities_storage[i];
	}
	entities_len = 0;
}

entity_type_t entity_type_by_name(char *type_name) {
	for (uint32_t type = ENTITY_TYPE_NONE+1; type < ENTITY_TYPES_COUNT; type++) {
		if (str_equals(type_name, entity_type_names[type])){
			return type;
		}
	}
	return ENTITY_TYPE_NONE;
}

const char *entity_type_name(entity_type_t type) {
	if (entities_len >= ENTITIES_MAX) {
		return NULL;
	}
	return entity_type_names[type];
}

void entities_update(void) {
	double start = platform_now();
	// Update all entities
	for (int i = 0; i < entities_len; i++) {
		entity_t *ent = entities[i];
		entity_update(ent);

		if (!ent->is_alive) {
			// If this entity is dead overwrite it with the last one and
			// decrease count.			
			entities_len--;
			if (i < entities_len) {
				entity_t *last = entities[entities_len];
				entities[entities_len] = entities[i];
				entities[i] = last;
				i--;
			}
		}
	}

	// Sort by x or y position - insertion sort
	#define COMPARE_POS(a, b) (a->pos.ENTITY_SWEEP_AXIS > b->pos.ENTITY_SWEEP_AXIS)
	sort(entities, entities_len, COMPARE_POS);
		
	// Sweep touches
	engine.perf.checks = 0;
	for (int i = 0; i < entities_len; i++) {
		entity_t *e1 = entities[i];

		if (
			e1->check_against != ENTITY_GROUP_NONE ||
			e1->group != ENTITY_GROUP_NONE ||
			((int)e1->physics > ENTITY_COLLIDES_LITE)
		) {
			float max_pos = e1->pos.ENTITY_SWEEP_AXIS + e1->size.ENTITY_SWEEP_AXIS;
			for (int j = i + 1; j < entities_len && entities[j]->pos.ENTITY_SWEEP_AXIS < max_pos; j++) {
				entity_t *e2 = entities[j];
				engine.perf.checks++;

				if (entity_is_touching(e1, e2)) {
					if (e1->check_against & e2->group) {
						entity_touch(e1, e2);
					}
					if (e1->group & e2->check_against) {
						entity_touch(e2, e1);
					}

					if (
						(int)e1->physics >= ENTITY_COLLIDES_LITE && 
						(int)e2->physics >= ENTITY_COLLIDES_LITE &&
						(e1->physics + e2->physics) >= (ENTITY_COLLIDES_ACTIVE | ENTITY_COLLIDES_LITE) &&
						e1->mass + e2->mass > 0
					) {
						entity_resolve_collision(e1, e2);
					}
				}
			}
		}
	}

	engine.perf.entities = entities_len;
}

bool entity_is_touching(entity_t *self, entity_t *other) {	
	return !(
		self->pos.x >= other->pos.x + other->size.x ||
		self->pos.x + self->size.x <= other->pos.x ||
		self->pos.y >= other->pos.y + other->size.y ||
		self->pos.y + self->size.y <= other->pos.y
	);
}

void entities_draw(vec2_t viewport) {
	// Sort entities by draw_order
	// FIXME: this copies the entity array - which is sorted by pos.x/y and
	// sorts it again by draw_order. It's using insertion sort, which is slow
	// for data that is not already mostly sorted.
	entity_t **draw_ents = bump_alloc(sizeof(entity_t *) * entities_len);
	memcpy(draw_ents, entities, entities_len * sizeof(entity_t*));
	
	#define SORT_DRAW_ORDER(a, b) (a->draw_order > b->draw_order)
	sort(draw_ents, entities_len, SORT_DRAW_ORDER);

	for (int i = 0; i < entities_len; i++) {
		entity_t *ent = draw_ents[i];
		entity_draw(ent, viewport);
	}
}

entity_t *entity_by_name(char *name) {
	// FIXME:PERF: linear search
	for (int i = 0; i < entities_len; i++) {
		entity_t *entity = entities[i];
		if (entity->is_alive && entity->name && str_equals(name, entity->name)) {
			return entity;
		}
	}

	return NULL;
}

entity_list_t entities_by_proximity(entity_t *ent, float radius, entity_type_t type) {
	vec2_t pos = entity_center(ent);
	return entities_by_location(pos, radius, type, ent);
}


entity_list_t entities_by_location(vec2_t pos, float radius, entity_type_t type, entity_t *exclude) {
	entity_list_t list = {.len = 0, .entities = bump_alloc(0)};

	float start_pos = pos.ENTITY_SWEEP_AXIS - radius;
	float end_pos = start_pos + radius * 2;

	float radius_squared = radius * radius;

	// Binary search to the last entity that is below ENTITY_MAX_SIZE of the
	// start point
	int lower_bound = 0;
	int upper_bound = entities_len - 1;
	float search_pos = start_pos - ENTITY_MAX_SIZE;
	while (lower_bound <= upper_bound) {
		int current_index = (lower_bound + upper_bound) / 2;
		float current_pos = entities[current_index]->pos.ENTITY_SWEEP_AXIS;

		if (current_pos < search_pos) {
			lower_bound = current_index + 1;
		}
		else if (current_pos > search_pos) {
			upper_bound = current_index - 1;
		}
		else {
			break;
		}
	}

	// Find entities in the sweep range
	for (int i = max(upper_bound, 0); i < entities_len; i++) {
		entity_t *entity = entities[i];
		
		// Have we reached the end of the search range?
		if (entity->pos.ENTITY_SWEEP_AXIS > end_pos) {
			break;
		}

		// Is this entity in the search range and has the right type?
		if (
			entity->pos.ENTITY_SWEEP_AXIS + entity->size.ENTITY_SWEEP_AXIS >= start_pos &&
			entity != exclude &&
			(type == ENTITY_TYPE_NONE || entity->type == type) &&
			entity->is_alive
		) {
			// Is the bounding box in the radius?
			float xd = entity->pos.x + (entity->pos.x < pos.x ? entity->size.x : 0) - pos.x;
			float yd = entity->pos.y + (entity->pos.y < pos.y ? entity->size.y : 0) - pos.y;
			if ((xd * xd) + (yd * yd) <= radius_squared) {
				bump_alloc(sizeof(entity_ref_t));
				list.entities[list.len++] = entity_ref(entity);
			}
		}
	}
		
	return list;
}

entity_list_t entities_by_type(entity_type_t type) {
	entity_list_t list = {.len = 0, .entities = bump_alloc(0)};

	// FIXME:PERF: linear search
	for (int i = 0; i < entities_len; i++) {
		entity_t *entity = entities[i];
		if (entity->type == type && entity->is_alive) {
			bump_alloc(sizeof(entity_ref_t));
			list.entities[list.len++] = entity_ref(entity);
		}
	}

	return list;
}

entity_list_t entities_from_json_names(json_t *targets) {
	entity_list_t list = {.len = 0, .entities = bump_alloc(0)};

	for (int i = 0; targets && i < targets->len; i++) {
		char *target_name = json_string(&targets->values[i]);
		entity_t *target = entity_by_name(target_name);
		if (target) {
			bump_alloc(sizeof(entity_ref_t));
			list.entities[list.len++] = entity_ref(target);
		}
	}
	return list;
}

entity_ref_t entity_ref(entity_t *self) {
	if (!self) {
		return entity_ref_none();
	}
	return (entity_ref_t){
		.id = self->id,
		.index = (uint32_t)(self - entities_storage)
	};
}

entity_t *entity_by_ref(entity_ref_t ref) {
	entity_t *ent = &entities_storage[ref.index];
	if (ent->is_alive && ent->id == ref.id) {
		return ent;
	}

	return NULL;
}

entity_t *entity_spawn(entity_type_t type, vec2_t pos) {
	if (entities_len >= ENTITIES_MAX) {
		return NULL;
	}

	entity_t *ent = entities[entities_len];
	entities_len++;
	entity_unique_id++;

	memset(ent, 0, sizeof(entity_t));
	ent->type = type;
	ent->id = entity_unique_id;
	ent->is_alive = true;
	ent->pos = pos;
	ent->max_ground_normal = 0.69; // cosf(to_radians(46));
	ent->min_slide_normal = 1; // cosf(to_radians(0));
	ent->gravity = 1;
	ent->mass = 1;
	ent->size = vec2(8, 8);

	entity_init(ent);
	return ent;
}

vec2_t entity_center(entity_t *ent) {
	return vec2_add(ent->pos, vec2_mulf(ent->size, 0.5));
}

float entity_dist(entity_t *a, entity_t *b) {
	return vec2_dist(entity_center(a), entity_center(b));
}

float entity_angle(entity_t *a, entity_t *b) {
	return vec2_angle(entity_center(a), entity_center(b));
}

void entity_base_damage(entity_t *self, entity_t *other, float damage) {
	self->health -= damage;

	if (self->health <= 0 && self->is_alive) {
		entity_kill(self);
	}
}

void entity_base_draw(entity_t *self, vec2_t viewport) {
	if (self->anim.def != NULL) {
		anim_draw(&self->anim, vec2_sub(vec2_sub(self->pos, viewport), self->offset));
	}
}

void entity_base_update(entity_t *self) {
	if (!(self->physics & ENTITY_PHYSICS_MOVE)) {
		return;
	}

	// Integrate velocity
	vec2_t v = self->vel;

	self->vel.y += engine.gravity * self->gravity * engine.tick;
	vec2_t friction = vec2(min(self->friction.x * engine.tick, 1), min(self->friction.y * engine.tick, 1));
	self->vel = vec2_add(
		self->vel, 
		vec2_sub(
			vec2_mulf(self->accel, engine.tick),
			vec2_mul(self->vel, friction)
		)
	);

	vec2_t vstep = vec2_mulf(vec2_add(v, self->vel), engine.tick * 0.5);
	self->on_ground = false;
	entity_move(self, vstep);
}

static void entity_move(entity_t *self, vec2_t vstep) {
	if ((self->physics & ENTITY_PHYSICS_WORLD) && engine.collision_map) {
		trace_t t = trace(engine.collision_map, self->pos, vstep, self->size);
		entity_handle_trace_result(self, &t);

		// The previous trace was stopped short and we still have some velocity
		// left? Do a second trace with the new velocity. this allows us
		// to slide along tiles;
		if (t.length < 1) {
			vec2_t rotated_normal = vec2(-t.normal.y, t.normal.x);
			float vel_along_normal = vec2_dot(vstep, rotated_normal);

			if (vel_along_normal != 0) {
				float remaining = 1 - t.length;
				vec2_t vstep2 = vec2_mulf(rotated_normal, vel_along_normal * remaining);	
				trace_t t2 = trace(engine.collision_map, self->pos, vstep2, self->size);
				entity_handle_trace_result(self, &t2);
			}
		}
	}
	else {
		self->pos = vec2_add(self->pos, vstep);
	}
}


static void entity_handle_trace_result(entity_t *self, trace_t *t) {
	self->pos = t->pos;

	if (!t->tile) {
		return;
	}

	entity_collide(self, t->normal, t);

	// If this entity is bouncy, calculate the velocity against the
	// slope's normal (the dot product) and see if we want to bounce
	// back.
	if (self->restitution > 0) {
		float vel_against_normal = vec2_dot(self->vel, t->normal);

		if (fabsf(vel_against_normal) * self->restitution > ENTITY_MIN_BOUNCE_VELOCITY) {
			vec2_t vn = vec2_mulf(t->normal, vel_against_normal * 2);
			self->vel = vec2_mulf(vec2_sub(self->vel, vn), self->restitution);
			return;
		}
	}

	// If this game has gravity, we may have to set the on_ground flag.
	if (engine.gravity && t->normal.y < -self->max_ground_normal) {
		self->on_ground = true;

		// If we don't want to slide on slopes, we cheat a bit by
		// fudging the y velocity.
		if (t->normal.y < -self->min_slide_normal) {
			self->vel.y = self->vel.x * t->normal.x;
		}
	}
	
	// Rotate the normal vector by 90° ([nx, ny] -> [-ny, nx]) to get
	// the slope vector and calculate the dot product with the velocity.
	// This is the velocity with which we will slide along the slope.
	vec2_t rotated_normal = vec2(-t->normal.y, t->normal.x);
	float vel_along_normal = vec2_dot(self->vel, rotated_normal);
	self->vel = vec2_mulf(rotated_normal, vel_along_normal);
}

static void entity_resolve_collision(entity_t *a, entity_t *b) {
	float overlap_x = a->pos.x < b->pos.x 
		? a->pos.x + a->size.x - b->pos.x
		: b->pos.x + b->size.x - a->pos.x;

	float overlap_y = a->pos.y < b->pos.y
		? a->pos.y + a->size.y - b->pos.y
		: b->pos.y + b->size.y - a->pos.y;

	float a_move;
	float b_move;
	if (
		a->physics & ENTITY_COLLIDES_LITE ||
		b->physics & ENTITY_COLLIDES_FIXED
	) {
		a_move = 1;
		b_move = 0;
	}
	else if (
		a->physics & ENTITY_COLLIDES_FIXED ||
		b->physics & ENTITY_COLLIDES_LITE
	) {
		a_move = 0;
		b_move = 1;
	}
	else {
		float total_mass = a->mass + b->mass;
		a_move = b->mass / total_mass;
		b_move = a->mass / total_mass;
	}

	if (overlap_y > overlap_x) {
		if (a->pos.x < b->pos.x) {
			entities_separate_on_x_axis(a, b, a_move, b_move, overlap_x);
			entity_collide(a, vec2(-1, 0), NULL);
			entity_collide(b, vec2( 1, 0), NULL);
		}
		else {
			entities_separate_on_x_axis(b, a, b_move, a_move, overlap_x);
			entity_collide(a, vec2( 1, 0), NULL);
			entity_collide(b, vec2(-1, 0), NULL);
		}
	}
	else {
		if (a->pos.y < b->pos.y) {
			entities_separate_on_y_axis(a, b, a_move, b_move, overlap_y);
			entity_collide(a, vec2(0, -1), NULL);
			entity_collide(b, vec2(0,  1), NULL);
		}
		else {
			entities_separate_on_y_axis(b, a, b_move, a_move, overlap_y);
			entity_collide(a, vec2(0,  1), NULL);
			entity_collide(b, vec2(0, -1), NULL);
		}
	}
}

static void entities_separate_on_x_axis(
	entity_t *left, entity_t *right, 
	float left_move, float right_move, float overlap
) {

	float impact_velocity = left->vel.x - right->vel.x;
	float left_vel_y = left->vel.x;

	if (left_move > 0) {
		left->vel.x = right->vel.x * left_move + left->vel.x * right_move;

		float bounce = impact_velocity * left->restitution;
		if (bounce > ENTITY_MIN_BOUNCE_VELOCITY) {
			left->vel.x -= bounce;
		}
		entity_move(left, vec2(-overlap * left_move, 0));
	}
	if (right_move > 0) {
		right->vel.x = left->vel.x * right_move + right->vel.x * left_move;

		float bounce = impact_velocity * right->restitution;
		if (bounce > ENTITY_MIN_BOUNCE_VELOCITY) {
			right->vel.x += bounce;
		}
		entity_move(right, vec2(overlap * right_move, 0));
	}
}

static void entities_separate_on_y_axis(
	entity_t *top, entity_t *bottom, 
	float top_move, float bottom_move, float overlap
) {
	if (bottom->on_ground && top_move > 0) {
		top_move = 1;
		bottom_move = 0;
	}

	float impact_velocity = top->vel.y - bottom->vel.y;
	float top_vel_y = top->vel.y;

	if (top_move > 0) {
		top->vel.y = (top->vel.y * bottom_move + bottom->vel.y * top_move);
		
		float move_x = 0;
		float bounce = impact_velocity * top->restitution;
		if (bounce > ENTITY_MIN_BOUNCE_VELOCITY) {
			top->vel.y -= bounce;
		}
		else {
			top->on_ground = true;
			move_x = bottom->vel.x * engine.tick;
		}
		entity_move(top, vec2(move_x, -overlap * top_move));
	}
	if (bottom_move > 0) {
		bottom->vel.y = bottom->vel.y * top_move + top_vel_y * bottom_move;

		float bounce = impact_velocity * bottom->restitution;
		if (bounce > ENTITY_MIN_BOUNCE_VELOCITY) {
			bottom->vel.y += bounce;
		}
		entity_move(bottom, vec2(0, overlap * bottom_move));
	}
}


