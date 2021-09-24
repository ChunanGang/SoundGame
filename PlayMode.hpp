#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <chrono>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;
	bool mouse_left = false;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// transform pointers
	Scene::Transform *mouse = nullptr;
	std::vector<Scene::Transform *> carrots; 

	// transform infos
	glm::quat mouse_offset_rotation;
	glm::vec3 mouse_offset_forward;
	glm::quat mouse_move_roration = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec2 mousePosMax = glm::vec2(-35, 119);
	glm::vec2 mousePosMin = glm::vec2(-305, -61);

	// game logic
	const uint16_t carrotNum = 25;
	bool collected[25] = {false};
	uint16_t curCarrotIndex = 30; // current pickable carrot (30 means no curCarrot)
	float currotOriginalHeight; 
	bool carrotRising = false;
	float carrotRiseSpeed = 5.0f;
	float curCarrotRiseAmount = 0; // how much cur carrot has rised
	const float carrotRiseAmountMax = 10.0f;
	bool popClickHint = false;
	uint16_t displayGameOverText=0; // 0 for no display, 1 for losing, 2 for winning
	bool winning = true;
	// mouse
	float ratateSpeed = 70.0f; // degree
	float moveSpeed = 40.0f;

	// game logic - functions
	void checkCarrotIndex();
	void riseCarrot(float elapsed);
	void collectCarrot(uint16_t index);
	bool gameOver();
	void playEndGameAnim(float elapsed);

	// mouse color effect
	float mouse_color_modifier = 0.0f;
	uint16_t changeSpeedFactor = 0;
	// carrot color effect
	uint32_t carrot_flash_interval = 100; // flash every this much ms

	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

};