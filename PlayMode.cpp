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

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

PlayMode::PlayMode() : scene(*hexapod_scene) {
	carrots = std::vector<Scene::Transform *>(carrotNum);
	//get pointers for convenience:
	for (auto &transform : scene.transforms) {
		std::cout << "(" << transform.name <<  ")" << std::endl;
		// mouse
		if (transform.name == "water opossum ") mouse = &transform;
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

	// disable mouse
	if (SDL_GetRelativeMouseMode() == SDL_FALSE) 
		SDL_SetRelativeMouseMode(SDL_TRUE);

	auto carrotWorldMat = carrots[0]->make_local_to_world();
	glm::vec3 carrotWorldPos(carrotWorldMat[3][0], carrotWorldMat[3][1],carrotWorldMat[3][2]);
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, carrotWorldPos);

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
			winning = false;
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

void PlayMode::checkCarrotIndex(){
	// if no current carrot, pick one and start rise
	if (curCarrotIndex == 30){
		// rejection sampling
		while(curCarrotIndex == 30){
			curCarrotIndex = rand() % carrotNum;
			if(collected[curCarrotIndex])
				curCarrotIndex = 30;
		}
		carrotRising = true;
		curCarrotRiseAmount = 0;
	}
}

void PlayMode::riseCarrot(float elapsed){
	// not high enough
	if(curCarrotRiseAmount <= carrotRiseAmountMax){
		curCarrotRiseAmount += elapsed * carrotRiseSpeed;
		carrots[curCarrotIndex]->position.y = currotOriginalHeight + curCarrotRiseAmount;
		// add a flashing color effect when rising
		std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
		auto duration = now.time_since_epoch();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
		// flash
		if (millis % (carrot_flash_interval*2) > carrot_flash_interval)
			carrots[curCarrotIndex]->colorModifier = glm::vec3(0.3f,0,0);
		else
			carrots[curCarrotIndex]->colorModifier = glm::vec3(0,0.3f,0);
	}
	// rised to position
	else {
		carrotRising = false;
		// set color modifier
		carrots[curCarrotIndex]->colorModifier = glm::vec3(0.5,0.5,0);
	}
}

void PlayMode::collectCarrot(uint16_t index){
	// set carrots invisible
	carrots[index]->position.z = 5000;
	// set as colected
	collected[index] = true;
	// speed up
	if (changeSpeedFactor < 10){
		changeSpeedFactor += 1;	// color change faster
		moveSpeed += 8;
		ratateSpeed += 8;
	}
	// reset values
	curCarrotIndex = 30;
}

bool PlayMode::gameOver(){
	for (size_t i =0; i <carrots.size(); i++){
		if(!collected[i])
			return false;
	}
	return true;
}

void PlayMode::playEndGameAnim(float elapsed){
	bool startFall=false;
	// first scale up
	float scaleSpeed = 3;
	float scaleMax = 6;
	if(mouse->scale.x < scaleMax){
		mouse->scale.x += scaleSpeed * elapsed;
		mouse->scale.y += scaleSpeed * elapsed;
		mouse->scale.z += scaleSpeed * elapsed;
	}
	else{
		if(! winning)
			startFall = true;
		else
			displayGameOverText = 2;
	}
	// then fall down
	if(startFall){
		static float rotatedAmount = 0;
		float rotateSpeed = 50;
		if(rotatedAmount < 90){
			rotatedAmount += elapsed * rotateSpeed;
			auto fall_roration = glm::angleAxis(
				glm::radians(rotatedAmount),
				glm::vec3(0,1,0));
			// combine rotation from offset and from player control 
			mouse->rotation = fall_roration * mouse_move_roration * mouse_offset_rotation;
		}
		// done with rotation, display game over text
		else
			displayGameOverText = 1;
	}
}

void PlayMode::update(float elapsed) {

	// --------- mouse color effect ------- //
	if(changeSpeedFactor == 0)
		mouse->colorModifier = glm::vec3(0);
	else{
		mouse_color_modifier += elapsed /10 * changeSpeedFactor;
		mouse_color_modifier -= std::floor(mouse_color_modifier);
		mouse->colorModifier = glm::vec3(
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (mouse_color_modifier + 0.0f / 3.0f) ) ) ))) /255.0f,
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (mouse_color_modifier + 1.0f / 3.0f) ) ) ))) /255.0f,
			std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (mouse_color_modifier + 2.0f / 3.0f) ) ) ))) /255.0f
		);
	}

	// check if game over
	if(gameOver()){
		playEndGameAnim(elapsed);
		return;
	}

	// ---------- carrots ------------- //
	checkCarrotIndex();
	// rise carrot
	riseCarrot(elapsed);

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

	// --------- pick up carrot ----------- //
	popClickHint = false;
	if (curCarrotIndex != 30 && !carrotRising){
		// get the world position of mouse and carrot
		auto mouseWorldMat = mouse->make_local_to_world();
		glm::vec3 mouseWorldPos(mouseWorldMat[3][0], mouseWorldMat[3][1],mouseWorldMat[3][2]);
		auto carrotWorldMat = carrots[curCarrotIndex]->make_local_to_world();
		glm::vec3 carrotWorldPos(carrotWorldMat[3][0], carrotWorldMat[3][1],carrotWorldMat[3][2]);
		//std::cout << "mouse: " << mouseWorldPos.x<< ", cr: " << carrotWorldPos.x <<std::endl;
		// check if in range
		if (abs(mouseWorldPos.x - carrotWorldPos.x) < 25 &&
			abs(mouseWorldPos.y - carrotWorldPos.y) < 25)
		{
			// change carrot color
			carrots[curCarrotIndex]->colorModifier = glm::vec3(0.6,0.0f,0.3f);
			popClickHint = true;
			// check for pick up input
			if (mouse_left){
				collectCarrot(curCarrotIndex);
			}
		}

		for(auto & carrot : carrots){
			auto carrotWorldM = carrot->make_local_to_world();
			glm::vec3 carrotWorldPos(carrotWorldM[3][0], carrotWorldM[3][1],carrotWorldM[3][2]);
			if (abs(mouseWorldPos.x - carrotWorldPos.x) < 10 &&
				abs(mouseWorldPos.y - carrotWorldPos.y) < 10)
				carrot->colorModifier = glm::vec3(0.0,1.0f,0.0f);
			else
				carrot->colorModifier = glm::vec3(0.0,0.0f,0.0f);
		}
	}

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
		lines.draw_text("WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		
		if(popClickHint && displayGameOverText == 0){
			lines.draw_text("LEFT CLICK",
				glm::vec3(-aspect + 0.1f * H , -1.0 + 0.1f * H + 0.2f, 0.0),
				glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.0f, 0.5f, 0.0f),
				glm::u8vec4(0xff, 0xff, 0x00, 0x00));
		}

		// game over text
		// lose
		if(displayGameOverText == 1)
			lines.draw_text("YOU DIE FROM CHEMICAL POISON",
				glm::vec3(-aspect + 0.1f * H , -1.0 + 0.1f * H + 0.8f, 0.0),
				glm::vec3(0.32f, 0.0f, 0.0f), glm::vec3(0.0f, 0.32f, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));
		// win
		else if (displayGameOverText == 2)
			lines.draw_text("YOU BECOME SUPER CHEMICAL MOUSE",
				glm::vec3(-aspect + 0.1f * H , -1.0 + 0.1f * H + 0.8f, 0.0),
				glm::vec3(0.3f, 0.0f, 0.0f), glm::vec3(0.0f, 0.32f, 0.0f),
				glm::u8vec4(0x00, 0x00, 0xff, 0x00));

	}
}