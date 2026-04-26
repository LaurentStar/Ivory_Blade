// WallRunScoring.cpp
//
// Computes normalized scores (0..1) for horizontal and vertical wall-run modes.
// Each score is a weighted blend of four "soft bool" factors derived from dot
// products (input direction, camera forward, character direction) and speed.
// A value inside its threshold range contributes 1.0; outside contributes 0.1
// rather than 0, so near-miss inputs still have partial influence.
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
	constexpr float horizontal_character_direction_min  = -0.6f;
	constexpr float horizontal_character_direction_max  =  0.3f;
	constexpr float horizontal_speed_min               = 600.0f;
	constexpr float horizontal_speed_max               = 80000.0f;

	constexpr float vertical_input_direction_min       = -1.0f;
	constexpr float vertical_input_direction_max       = -0.6f;
	constexpr float vertical_camera_forward_min        = -1.0f;
	constexpr float vertical_camera_forward_max        = -0.8f;
	constexpr float vertical_character_direction_min    = -1.0f;
	constexpr float vertical_character_direction_max    =  0.0f;
	constexpr float vertical_speed_min                 = 300.0f;
	constexpr float vertical_speed_max                 = 80000.0f;

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
	float& horizontal_score,
	float& vertical_score,
	float input_direction_weight,
	float camera_forward_weight,
	float character_direction_weight,
	float speed_weight)
{
	horizontal_score = compute_normalized_score(
		input_direction_dot_product, camera_forward_dot_product,
		character_direction_dot_product, current_speed,
		input_direction_weight, camera_forward_weight,
		character_direction_weight, speed_weight,
		horizontal_input_direction_min, horizontal_input_direction_max,
		horizontal_camera_forward_min,  horizontal_camera_forward_max,
		horizontal_character_direction_min, horizontal_character_direction_max,
		horizontal_speed_min, horizontal_speed_max);

	vertical_score = compute_normalized_score(
		input_direction_dot_product, camera_forward_dot_product,
		character_direction_dot_product, current_speed,
		input_direction_weight, camera_forward_weight,
		character_direction_weight, speed_weight,
		vertical_input_direction_min, vertical_input_direction_max,
		vertical_camera_forward_min,  vertical_camera_forward_max,
		vertical_character_direction_min, vertical_character_direction_max,
		vertical_speed_min, vertical_speed_max);
}
