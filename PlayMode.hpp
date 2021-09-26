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
	Scene::Transform *ball = nullptr;
	std::vector<Scene::Transform *> carrots; 

	// transform infos
	glm::quat mouse_offset_rotation;
	glm::vec3 mouse_offset_forward;
	glm::quat mouse_move_roration = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec2 mousePosMax = glm::vec2(-35, 119);
	glm::vec2 mousePosMin = glm::vec2(-305, -61);

	// ----- game logic -----
	// carrot
	const uint16_t carrotNum = 18;
	bool collected[18] = {false};
	uint16_t curCarrotIndex = 0; // current light-up carrot
	uint16_t corrrectCarrotIndex = -1; // the carrot that should be picked
	float changeCarrotInterval = 1.5f;
	float currotOriginalHeight; 
	uint16_t carrotsNeeded = 10;
	// mouse
	float ratateSpeed = 120.0f; // degree
	float moveSpeed = 100.0f;
	// game play
	enum GameState{
		PlayingMusic = 0,
		Selecting = 1,
		WaitToStart = 2,
	};
	GameState gameState = WaitToStart;
	float waitTime = 3.0f; // how long to wait before srating next round
	uint16_t displayText=11; 
	bool winning = true;
	glm::vec2 musicLengthInterval = glm::vec2(5.0f, 12.0f); // how long will the music play: (min, max)
	glm::vec2 lightUpExtraTime = glm::vec2(2.0f, 6.0f); // how long will the carrots keep being lightup after music stoped
	float curMusicLength;	// how long music play
	float curCarrotLightUpLenght;	// hopw long carrots are on light-up rotation

	// ----- game logic - functions -----
	void playerMovement(float elapsed);
	void collectCarrot(uint16_t index);
	bool gameOver();
	void changeCarrot(float elapsed, bool reset);
	void lightUpCurCarrot();
	void setUpNextRound();
	void checkForStop(float elapsed);
	bool lastWasCorrect = true;
	void showCorrectCarrot(float elapsed);
	// helper
	glm::vec3 carrotWorldPos(size_t index);

	// color effect
	float mouse_color_modifier = 0.0f;
	// carrot colors
	const glm::vec3 lightUpColor = glm::vec3(0,1,1);
	const glm::vec3 toBeSelectColor = glm::vec3(0.1,0.2,0.2);
	const glm::vec3 SelectedColor = glm::vec3(0,0,1);
	// ball color
	const glm::vec3 ballDefaultColor = glm::vec3(0.1,0.2,0.4);
	// 
	float carrotBlinkModifier = 0.0f;
	float ballBlinkModifier = 0.0f;
	

	std::shared_ptr< Sound::PlayingSample > music;
	
	//camera:
	Scene::Camera *camera = nullptr;

};