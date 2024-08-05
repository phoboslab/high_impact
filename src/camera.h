#ifndef HI_CAMERA_H
#define HI_CAMERA_H

// A camera allows you to follow entities and move smoothly to a new position.
// You can also specify a deadzone and lookahead to move the viewport closer
// to the action. Using a camera is totally optional; you could instead
// manipulate the engine.viewport directly if you wish.

// Cameras can be instantiated from just a camera_t, i.e.:

// camera_t cam;
// camera_follow(&cam, some_entity, true);

// To actually move the camera, you have to call camera_update(). This is 
// typically done once per frame.

// If the engine.collision_map is set, the camera will ensure the screen stays
// within the bounds of this map.

#include "types.h"
#include "entity_def.h"

typedef struct {
	// A factor of how fast the camera is moving. Values between 0.5..10
	// are usually sensible.
	float speed;

	// A fixed offset of the screen center from the target entity.
	vec2_t offset;

	// Whether to automatically move the bottom of the deadzone up to the
	// target entity when the target is on_ground
	bool snap_to_platform;

	// The minimum velocity (in pixels per second) for a camera movement. If 
	// this is set too low and the camera is close to the target it will move 
	// very slowly which results in a single pixel movement every few moments, 
	// which can look weird. 5 looks good, imho.
	float min_vel;

	// The size of the deadzone: the size of the area around the target within 
	// which the camera will not move. The camera will move only when the target
	// is about to leave the deadzone.
	vec2_t deadzone;

	// The amount of pixels the camera should be ahead the target. Whether the
	// "ahead" means left/right (or above/below), is determined by the edge of 
	// the deadzone that the entity touched last.
	vec2_t look_ahead;


	// Internal state
	vec2_t deadzone_pos;
	vec2_t look_ahead_target;
	entity_ref_t follow;
	vec2_t pos;
	vec2_t vel;
} camera_t;

// Advance the camera towards its target
void camera_update(camera_t *cam);

// Set the camera to pos (no movement)
void camera_set(camera_t *cam, vec2_t pos);

// Set the target to pos
void camera_move(camera_t *cam, vec2_t pos);

// Follow an entity. Set snap to true when you want to jump to it. The camera
// will follow this target for as long as it's alive (or until following 
// another entity / unfollow)
void camera_follow(camera_t *cam, entity_ref_t follow, bool snap);

// Stop following the entity
void camera_unfollow(camera_t *cam);

#endif
