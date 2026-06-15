// WallRunScoring.cpp
//
// Computes normalized scores (0..1) for horizontal and vertical wall-run modes.
// Each score is a weighted blend of five "soft bool" factors: input direction,
// camera forward, character direction, speed, and fall speed (dot(velocity,
// local_up)).  A value inside its threshold range contributes 1.0; outside
// contributes 0.1 so near-miss inputs still have partial influence.
//
// The fall speed factor uses dual weights: fall_speed_weight_true (when in
// range) and fall_speed_weight_false (when out of range).  A small true weight
// minimizes score buffering; a large false weight heavily penalizes falling
// too fast relative to local gravity.
//
// Called per-frame from VRPawn Blueprint to decide which wall-run mode to use.

#include "WallRunScoring.h"

namespace
{
	constexpr float soft_bool_in_range     = 1.0f;
	constexpr float soft_bool_out_of_range = 0.1f;

	constexpr float horizontal_input_direction_min     = -0.75f;
	constexpr float horizontal_input_direction_max     =  0.3f;
	constexpr float horizontal_camera_forward_min      = -0.8f;
	constexpr float horizontal_camera_forward_max      =  1.0f;
	constexpr float horizontal_character_direction_min  = -0.65f;
	constexpr float horizontal_character_direction_max  =  0.3f;
	constexpr float horizontal_speed_min               = 50.0f;
	constexpr float horizontal_speed_max               = 80000.0f;
	constexpr float horizontal_fall_speed_min          = -500.0f;
	constexpr float horizontal_fall_speed_max          = 80000.0f;

	constexpr float vertical_input_direction_min       = -1.0f;
	constexpr float vertical_input_direction_max       = -0.75f;
	constexpr float vertical_camera_forward_min        = -1.0f;
	constexpr float vertical_camera_forward_max        = -0.8f;
	constexpr float vertical_character_direction_min    = -1.0f;
	constexpr float vertical_character_direction_max    = -0.6f;
	constexpr float vertical_speed_min                 = 0.0f;
	constexpr float vertical_speed_max                 = 80000.0f;

	constexpr float vertical_fall_speed_min            = -500.0f;
	constexpr float vertical_fall_speed_max            = 80000.0f;

	float compute_soft_bool(float value, float min, float max)
	{
		return (value >= min && value <= max) ? soft_bool_in_range : soft_bool_out_of_range;
	}

	float compute_normalized_score(
		float input_direction_dot_product,
		float camera_forward_dot_product,
		float character_direction_dot_product,
		float current_speed,
		float input_direction_weight,
		float camera_forward_weight,
		float character_direction_weight,
		float speed_weight,
		float input_dir_min, float input_dir_max,
		float cam_fwd_min,   float cam_fwd_max,
		float char_dir_min,  float char_dir_max,
		float spd_min,       float spd_max)
	{
		const float input_direction_in_range     = compute_soft_bool(input_direction_dot_product,
		                                                             input_dir_min, input_dir_max);
		const float camera_forward_in_range      = compute_soft_bool(camera_forward_dot_product,
		                                                             cam_fwd_min, cam_fwd_max);
		const float character_direction_in_range = compute_soft_bool(character_direction_dot_product,
		                                                             char_dir_min, char_dir_max);
		const float speed_in_range               = compute_soft_bool(current_speed,
		                                                             spd_min, spd_max);

		const float weighted_sum = (input_direction_weight     * input_direction_in_range)
		                         + (camera_forward_weight      * camera_forward_in_range)
		                         + (character_direction_weight  * character_direction_in_range)
		                         + (speed_weight               * speed_in_range);

		const float weight_total = input_direction_weight + camera_forward_weight
		                         + character_direction_weight + speed_weight;

		return weighted_sum / weight_total;
	}
}

void UWallRunScoring::ComputeWallRunScores(
	float input_direction_dot_product,
	float camera_forward_dot_product,
	float character_direction_dot_product,
	float current_speed,
	float fall_speed,
	float& horizontal_score,
	float& vertical_score,
	float input_direction_weight,
	float camera_forward_weight,
	float character_direction_weight,
	float speed_weight,
	float fall_speed_weight_true,
	float fall_speed_weight_false)
{
	const float base_weight_total = input_direction_weight + camera_forward_weight
	                              + character_direction_weight + speed_weight;

	const float base_horizontal = compute_normalized_score(
		input_direction_dot_product, camera_forward_dot_product,
		character_direction_dot_product, current_speed,
		input_direction_weight, camera_forward_weight,
		character_direction_weight, speed_weight,
		horizontal_input_direction_min, horizontal_input_direction_max,
		horizontal_camera_forward_min,  horizontal_camera_forward_max,
		horizontal_character_direction_min, horizontal_character_direction_max,
		horizontal_speed_min, horizontal_speed_max);

	const float h_fall_in_range = compute_soft_bool(fall_speed,
	                                                horizontal_fall_speed_min,
	                                                horizontal_fall_speed_max);
	const float h_fall_weight = (h_fall_in_range == soft_bool_in_range)
	                          ? fall_speed_weight_true : fall_speed_weight_false;

	horizontal_score = (base_horizontal * base_weight_total + h_fall_weight * h_fall_in_range)
	                 / (base_weight_total + h_fall_weight);

	const float base_vertical = compute_normalized_score(
		input_direction_dot_product, camera_forward_dot_product,
		character_direction_dot_product, current_speed,
		input_direction_weight, camera_forward_weight,
		character_direction_weight, speed_weight,
		vertical_input_direction_min, vertical_input_direction_max,
		vertical_camera_forward_min,  vertical_camera_forward_max,
		vertical_character_direction_min, vertical_character_direction_max,
		vertical_speed_min, vertical_speed_max);

	const float v_fall_in_range = compute_soft_bool(fall_speed,
	                                                vertical_fall_speed_min,
	                                                vertical_fall_speed_max);
	const float v_fall_weight = (v_fall_in_range == soft_bool_in_range)
	                          ? fall_speed_weight_true : fall_speed_weight_false;

	vertical_score = (base_vertical * base_weight_total + v_fall_weight * v_fall_in_range)
	               / (base_weight_total + v_fall_weight);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT(
		"=== WallRun Scoring ===\n"
		"  Inputs   | InputDir: %.2f | CamFwd: %.2f | CharDir: %.2f | Speed: %.1f | FallSpd: %.1f\n"
		"  Horiz    | InDir: %.1f | CamFwd: %.1f | CharDir: %.1f | Spd: %.1f | Fall: %.1f (wt=%.1f) | Base: %.3f | Score: %.3f\n"
		"  Vert     | InDir: %.1f | CamFwd: %.1f | CharDir: %.1f | Spd: %.1f | Fall: %.1f (wt=%.1f) | Base: %.3f | Score: %.3f"),
		input_direction_dot_product, camera_forward_dot_product,
		character_direction_dot_product, current_speed, fall_speed,
		compute_soft_bool(input_direction_dot_product,  horizontal_input_direction_min,    horizontal_input_direction_max),
		compute_soft_bool(camera_forward_dot_product,   horizontal_camera_forward_min,     horizontal_camera_forward_max),
		compute_soft_bool(character_direction_dot_product, horizontal_character_direction_min, horizontal_character_direction_max),
		compute_soft_bool(current_speed,                horizontal_speed_min,              horizontal_speed_max),
		h_fall_in_range, h_fall_weight, base_horizontal, horizontal_score,
		compute_soft_bool(input_direction_dot_product,  vertical_input_direction_min,      vertical_input_direction_max),
		compute_soft_bool(camera_forward_dot_product,   vertical_camera_forward_min,       vertical_camera_forward_max),
		compute_soft_bool(character_direction_dot_product, vertical_character_direction_min, vertical_character_direction_max),
		compute_soft_bool(current_speed,                vertical_speed_min,                vertical_speed_max),
		v_fall_in_range, v_fall_weight, base_vertical, vertical_score);
#endif
}
