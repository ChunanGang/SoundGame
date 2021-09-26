#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("garden.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("garden.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;
		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > musicClip1(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("music1.opus"));
});

Load< Sound::Sample > musicClip2(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("music2.opus"));
});

Load< Sound::Sample > musicClip3(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("music3.opus"));
});

Load< Sound::Sample > correctSE(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("correct.opus"));
});

Load< Sound::Sample > wrongSE(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("wrong.opus"));
});

PlayMode::PlayMode() : scene(*hexapod_scene) {
	carrots = std::vector<Scene::Transform *>(carrotNum);
	//get pointers for convenience:
	for (auto &transform : scene.transforms) {
		//std::cout << "(" << transform.name <<  ")" << std::endl;
		// mouse
		if (transform.name == "water opossum ") mouse = &transform;
		// ball
		else if (transform.name == "Sphere.001") ball = &transform;
		// carrots
		else if (transform.name.find("carrot") !=std::string::npos){
			// add carrot in the right order (same as its name)
			uint16_t idx =  std::stoi(transform.name.substr(8,2));
			carrots[idx] = &transform;
		}
	}
	currotOriginalHeight = carrots[0]->position.y;
	if (mouse == nullptr) { int a; std::cin>>a; throw std::runtime_error("Hip not found.");}

	// set mouse offsets
	mouse_offset_rotation = mouse->rotation;
	mouse_offset_forward = mouse_offset_rotation * glm::vec4(glm::vec3(-1,0,0),0);

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	
	ball->colorModifier = ballDefaultColor;

}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			// ever moving backward will leak the chemical poison into your stomach. then you will die at the end :)
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} 

	// left click
	if(evt.type == SDL_MOUSEBUTTONDOWN)
		if (evt.button.button == SDL_BUTTON_LEFT)
			mouse_left = true;		
	if(evt.type == SDL_MOUSEBUTTONUP)
		if (evt.button.button == SDL_BUTTON_LEFT) 
			mouse_left = false;

	return false;
}

void PlayMode::collectCarrot(uint16_t index){
	//std::cout << "collecting "<<index <<std::endl;
	// correct
	if(index == corrrectCarrotIndex){
		lastWasCorrect = true;
		// set carrots "invisible"
		carrots[index]->position.z = 5000;
		// set as colected
		collected[index] = true;
		carrotsNeeded -= 1;
		// play se
		Sound::play_3D(*correctSE, 0.4f, carrotWorldPos(index));
		// make harder
		changeCarrotInterval -= 0.125f;
		displayText = 2;
	}
	// wrong
	else{
		Sound::play_3D(*wrongSE, .6f, carrotWorldPos(index));
		lastWasCorrect = false;
		displayText = 1;
	}
	// reset color effect
	for (auto c : carrots){
		c->colorModifier = glm::vec3(0);
	}
}

void PlayMode::showCorrectCarrot(float elapsed){
	carrotBlinkModifier += elapsed ;
	carrotBlinkModifier -= std::floor(carrotBlinkModifier);
	carrots[corrrectCarrotIndex]->colorModifier = glm::vec3(
		std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (carrotBlinkModifier + 0.0f / 3.0f) ) ) ))) /255.0f,
		std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (carrotBlinkModifier + 1.0f / 3.0f) ) ) ))) /255.0f,
		std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (carrotBlinkModifier + 2.0f / 3.0f) ) ) ))) /255.0f
	);
}

bool PlayMode::gameOver(){
	if(carrotsNeeded<=0)
		return true;
	else
		return false;
}

void PlayMode::changeCarrot(float elapsed, bool reset){
	static float timeOnCurrentCarrot = 0;
	if(reset) timeOnCurrentCarrot = 0;
	timeOnCurrentCarrot += elapsed;
	// time to move to next carrot
	if(timeOnCurrentCarrot >= changeCarrotInterval){
		curCarrotIndex = (curCarrotIndex +1) % carrotNum;
		// find the next non collected carrot
		while(collected[curCarrotIndex]){
			curCarrotIndex = (curCarrotIndex +1) % carrotNum;
		}
		timeOnCurrentCarrot =0;
	}
	// set pos of music
	music->set_position(carrotWorldPos(curCarrotIndex));
}

void PlayMode::lightUpCurCarrot(){
	for(size_t i =0; i<carrots.size(); i++){
		//std::cout << curCarrotIndex << ",  " << i <<std::endl;
		if(i == curCarrotIndex){
			carrots[i]->colorModifier = lightUpColor;
		}
		else{
			carrots[i]->colorModifier = glm::vec3(0,0,0);
		}
	}
}

void PlayMode::checkForStop(float elapsed){
	static float timeOnThisRound = 0;
	static bool musicPlaying = true;
	timeOnThisRound += elapsed;
	//stop music
	if(musicPlaying && timeOnThisRound >= curMusicLength){
		//std::cout<<"stop music\n";
		music->stop();
		corrrectCarrotIndex = curCarrotIndex;
		musicPlaying = false;
	}
	// stop carrot light-up rotation
	else if (timeOnThisRound >= curCarrotLightUpLenght){
		//std::cout<<"stop light\n";
		// move to next state
		gameState = Selecting;
		// reset color effects
		for(auto & carrot : carrots){
			carrot->colorModifier = toBeSelectColor;
		}
		ball->colorModifier = ballDefaultColor;
		//carrots[corrrectCarrotIndex]->colorModifier = glm::vec3(1,0,0);
		timeOnThisRound = 0;
		musicPlaying = true;
	}
}

inline float randFloatBetween(float min, float max){
	return min + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(max-min)));
}

glm::vec3 PlayMode::carrotWorldPos(size_t index){
	auto carrotWorldMat = carrots[index]->make_local_to_world();
	return glm::vec3(carrotWorldMat[3][0], carrotWorldMat[3][1],carrotWorldMat[3][2]);
}

void PlayMode::setUpNextRound(){
	// decide the timings
	curMusicLength = randFloatBetween(musicLengthInterval[0], musicLengthInterval[1]);
	curCarrotLightUpLenght = curMusicLength + randFloatBetween(lightUpExtraTime[0],lightUpExtraTime[1]);
	//std::cout << "starting next round. music: " << curMusicLength << ", lightup: " << curCarrotLightUpLenght <<std::endl;
	// start to play music
	// randomly choose from the 3 musics
	int m = rand()%3;
	if(m==0) 		
		music = Sound::play_3D(*musicClip1, 1.0f, carrotWorldPos(curCarrotIndex));
	else if (m==1) 	
		music = Sound::play_3D(*musicClip2, 1.0f, carrotWorldPos(curCarrotIndex));
	else 			
		music = Sound::play_3D(*musicClip3, 1.0f, carrotWorldPos(curCarrotIndex));
	// change state
	gameState = PlayingMusic;
}

void PlayMode::update(float elapsed) {

	// --------- mouse color effect ------- //

	// check if game over
	if(gameOver()){
		displayText = 3;
		return;
	}

	static bool reset = true;

	// allow to move
	playerMovement(elapsed);

	// depends on diff game state

	// -----  WAITING to start -----
	if(gameState == WaitToStart){
		// if wrong las time, show the correct carrot while waiting to start
		if(!lastWasCorrect)
			showCorrectCarrot(elapsed);
		static float curWaitTime = 0;
		curWaitTime += elapsed;
		// time to start next round
		if(curWaitTime >= waitTime){
			setUpNextRound();
			reset = true;
			curWaitTime = 0;
		}
	}

	// ----- PLAYING music -----
	else if(gameState == PlayingMusic){
		displayText = 10;
		// ball blinking
		ballBlinkModifier += elapsed ;
		ballBlinkModifier -= std::floor(ballBlinkModifier);
		ball->colorModifier = glm::vec3(
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (ballBlinkModifier + 0.0f / 3.0f) ) ) ))) /255.0f,
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (ballBlinkModifier + 1.0f / 3.0f) ) ) ))) /255.0f,
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (ballBlinkModifier + 2.0f / 3.0f) ) ) ))) /255.0f
		);
		// carrots rotation
		changeCarrot(elapsed, reset);
		reset = false;
		lightUpCurCarrot();
		checkForStop(elapsed);
	}

	// ----- SELECT -----
	else if (gameState == Selecting){
		displayText = 0;
		// check if any carrot in range
		int selectedCarrotIndex = -1;
		auto mouseWorldMat = mouse->make_local_to_world();
		glm::vec3 mousePos(mouseWorldMat[3][0], mouseWorldMat[3][1],mouseWorldMat[3][2]);
		for(size_t i =0; i<carrots.size(); i++){
			glm::vec3 carrotPos = carrotWorldPos(i);
			auto & carrot = carrots[i];
			if (abs(mousePos.x - carrotPos.x) < 10 && abs(mousePos.y - carrotPos.y) < 10){
				// change color and move up a bit
				carrot->colorModifier = SelectedColor;
				carrot->position.y = currotOriginalHeight + 2.0f;
				// record this index
				selectedCarrotIndex = (int)i;
			}
			else{
				carrot->colorModifier = toBeSelectColor;
				carrot->position.y = currotOriginalHeight;
			}
		}
		// check for the pick up input (left click)
		if (mouse_left && selectedCarrotIndex >= 0){
			collectCarrot(selectedCarrotIndex);
			gameState = WaitToStart;
		}
	}
}

void PlayMode::playerMovement(float elapsed){
	// ---------- movement -------------- //
	//combine inputs into a move:
	glm::vec2 move = glm::vec2(0.0f);
	if (left.pressed && !right.pressed) move.x =-1.0f;
	if (!left.pressed && right.pressed) move.x = 1.0f;
	if (down.pressed && !up.pressed) move.y =-1.0f;
	if (!down.pressed && up.pressed) move.y = 1.0f;

	// update left/right rotation from key A and D
	mouse_move_roration = glm::angleAxis(
		glm::radians(elapsed * ratateSpeed * move.x),
		glm::vec3(0,0,-1)) * mouse_move_roration;
	// combine rotation from offset and from player control 
	mouse->rotation = mouse_move_roration * mouse_offset_rotation;

	// move forward/backward from key W and S
	//make it so that moving diagonally doesn't go faster:
	if (move != glm::vec2(0.0f)) move = glm::normalize(move) * moveSpeed * elapsed;
	// calculate forward from the player-control rotation
	glm::vec3 forward = mouse_move_roration * mouse_offset_forward;
	mouse->position += move.y * forward;
	//std::cout << mouse->position.x<< ", " << mouse->position.y<< ", " <<  mouse->position.z<< ", " << std::endl;
	
	// clamp mousepos
	mouse->position.x = std::min(mouse->position.x, mousePosMax.x); 
	mouse->position.x = std::max(mouse->position.x, mousePosMin.x); 
	mouse->position.y = std::min(mouse->position.y, mousePosMax.y); 
	mouse->position.y = std::max(mouse->position.y, mousePosMin.y); 

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("WASD moves; left click to pick up",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H*1.5, 0.0f, 0.0f), glm::vec3(0.0f, H*1.5, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		
		lines.draw_text("Need " + std::to_string(carrotsNeeded)+ " more carrots",
			glm::vec3(-aspect + 0.1f * H + 0.1, -1.0 + 20 * H, 0.0),
			glm::vec3(H*1.5, 0.0f, 0.0f), glm::vec3(0.0f, H*1.5, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xee, 0x00));

		if( displayText == 0){
			lines.draw_text("Go find the correct carrot NOW",
				glm::vec3(-aspect + 0.1f * H , -1.0 + 0.1f * H + 0.2f, 0.0),
				glm::vec3(0.2f, 0.0f, 0.0f), glm::vec3(0.0f, 0.2f, 0.0f),
				glm::u8vec4(0xff, 0xff, 0x00, 0x00));
		}
		else if(displayText == 1)
			lines.draw_text("Should be the shining one",
				glm::vec3(-aspect + 0.1f * H , -1.0 + 0.1f * H + 0.8f, 0.0),
				glm::vec3(0.3f, 0.0f, 0.0f), glm::vec3(0.0f, 0.3f, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));
		else if(displayText == 2)
			lines.draw_text("Correct !!!",
				glm::vec3(-aspect + 0.1f * H , -1.0 + 0.1f * H + 0.8f, 0.0),
				glm::vec3(0.3f, 0.0f, 0.0f), glm::vec3(0.0f, 0.3f, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));
		else if (displayText == 3)
			lines.draw_text("MISSION COMPLETE",
				glm::vec3(-aspect + 0.f * H +0.2, -1.0 + 0.1f * H + 0.8f, 0.0),
				glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.0f, 0.5f, 0.0f),
				glm::u8vec4(0x00, 0x00, 0xff, 0x00));
		else if (displayText == 11)
			lines.draw_text("GET READY",
				glm::vec3(-aspect + 0.1f * H , -1.0 + 0.1f * H + 0.8f, 0.0),
				glm::vec3(0.3f, 0.0f, 0.0f), glm::vec3(0.0f, 0.3f, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));

	}
}