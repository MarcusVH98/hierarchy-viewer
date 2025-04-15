/*
 * Copyright (C) 2020, Inria
 * GRAPHDECO research group, https://team.inria.fr/graphdeco
 * All rights reserved.
 *
 * This software is free for non-commercial, research and evaluation use 
 * under the terms of the LICENSE.md file.
 *
 * For inquiries contact sibr@inria.fr and/or George.Drettakis@inria.fr
 */


#include <fstream>

#include <core/graphics/Window.hpp>
#include <core/view/MultiViewManager.hpp>
#include <core/system/String.hpp>
#include "projects/hierarchyviewer/renderer/HierarchyView.hpp" 

#include <core/renderer/DepthRenderer.hpp>
#include <core/raycaster/Raycaster.hpp>
#include <core/view/SceneDebugView.hpp>

#include <asio.hpp>
#include <nlohmann/json.hpp>
#include <atomic>

#define PROGRAM_NAME "sibr_3Dhierarchy"
using namespace sibr;

const char* usage = ""
"Usage: " PROGRAM_NAME " -path <dataset-path>"    	                                "\n"
;

using asio::ip::udp;
using json = nlohmann::json;

// Struct to hold camera transform data
struct CameraTransform {
	sibr::Vector3f position;
	sibr::Quaternionf rotation;
};

// Shared variable to store camera transform
std::shared_ptr<CameraTransform> cameraTransform = nullptr;
std::mutex cameraTransformMutex;

std::atomic<bool> _running {false};
std::atomic<bool> _newData {false};

// Function to update the absolute camera transform
void updateCameraTransform(const sibr::Vector3f& transform, const sibr::Quaternionf& rotation) {
	std::cout << "in updateCameraTransform" << std::endl;
	std::lock_guard<std::mutex> lock(cameraTransformMutex);
	if (!cameraTransform) {
		cameraTransform = std::make_shared<CameraTransform>();
	}
	cameraTransform->position = transform;
	cameraTransform->rotation = rotation;
}

// Function to translate the camera transform
void translateCamera(const sibr::Vector3f& translation, const sibr::Quaternionf& rotation) {
	std::cout << "in translateCamera" << std::endl;
	std::lock_guard<std::mutex> lock(cameraTransformMutex);
	if (!cameraTransform) {
		cameraTransform = std::make_shared<CameraTransform>();
	}
	cameraTransform->position += translation;
	cameraTransform->rotation = rotation;
}

void runUDPServer(std::atomic<bool>& _running) {
    try {
        asio::io_context io_context;
        udp::socket socket(io_context, udp::endpoint(udp::v4(), 4444));

        std::cout << "UDP Server started. Waiting for messages..." << std::endl;

        char data[1024];
        udp::endpoint sender_endpoint;

        while (_running) {
            asio::error_code error;
            size_t length = socket.receive_from(asio::buffer(data), sender_endpoint, 0, error);

            if (!error && length > 0) {
                std::string jsonStr(data, length);
                std::cout << "Received JSON: " << jsonStr << std::endl;

                // Parse JSON
                json jsonData = json::parse(jsonStr);

                // Absolute position update
                sibr::Vector3f position(
                    jsonData["position"]["x"],
                    jsonData["position"]["y"],
                    jsonData["position"]["z"]
                );
                sibr::Quaternionf rotation(
                    jsonData["rotation"]["w"],
                    jsonData["rotation"]["x"],
                    jsonData["rotation"]["y"],
                    jsonData["rotation"]["z"]
                );

                updateCameraTransform(position, rotation);

                _newData = true;
            } else if (error) {
                std::cerr << "Error receiving data: " << error.message() << std::endl;
            }
        }
    } catch (std::exception& e) {
        std::cerr << "UDP Server error: " << e.what() << std::endl;
    }
}

int main(int ac, char** av) {

	// Parse Command-line Args
	CommandLineArgs::parseMainArgs(ac, av);
	GaussianAppArgs myArgs;
	myArgs.displayHelpIfRequired();

	//const bool doVSync = !myArgs.vsync;
	myArgs.vsync = false;
	// rendering size
	uint rendering_width = myArgs.rendering_size.get()[0];
	uint rendering_height = myArgs.rendering_size.get()[1];
	
	// window size
	uint win_width = rendering_width; // myArgs.win_width;
	uint win_height = rendering_height; // myArgs.win_height;

	const char* toload = myArgs.modelPath.get().c_str();
	const char* scaffold = myArgs.scaffoldPath.get().c_str();

	bool udpEnabled = myArgs.tcpEnabled;

	// Window setup
	sibr::Window		window(PROGRAM_NAME, sibr::Vector2i(50, 50), myArgs, getResourcesDirectory() + "/hierarchy/" + PROGRAM_NAME + ".ini");

	sibr::BasicIBRScene::SceneOptions opts;
	opts.cameras = true;
	opts.images = false;
	opts.mesh = true;
	opts.renderTargets = false;
	opts.texture = false;

	BasicIBRScene::Ptr		scene(new BasicIBRScene(myArgs, opts));

	// Setup the scene: load the proxy, create the texture arrays.
	const uint flags = SIBR_GPU_LINEAR_SAMPLING | SIBR_FLIP_TEXTURE;

	// Fix rendering aspect ratio if user provided rendering size
	uint scene_width = scene->cameras()->inputCameras()[0]->w();
	uint scene_height = scene->cameras()->inputCameras()[0]->h();
	float scene_aspect_ratio = scene_width * 1.0f / scene_height;
	float rendering_aspect_ratio = rendering_width * 1.0f / rendering_height;

	if ((rendering_width > 0) && !myArgs.force_aspect_ratio ) {
		if (abs(scene_aspect_ratio - rendering_aspect_ratio) > 0.001f) {
			if (scene_width > scene_height) {
				rendering_height = rendering_width / scene_aspect_ratio;
			}
			else {
				rendering_width = rendering_height * scene_aspect_ratio;
			}
		}
	}
	
	// check rendering size
	rendering_width = (rendering_width <= 0) ? scene->cameras()->inputCameras()[0]->w() : rendering_width;
	rendering_height = (rendering_height <= 0) ? scene->cameras()->inputCameras()[0]->h() : rendering_height;
	Vector2u usedResolution(rendering_width, rendering_height);
	std::cerr << " USED RES " << usedResolution << " scene w h " << scene_width << " : " << scene_height <<  
		 " NAME " << scene->cameras()->inputCameras()[0]->name() << std::endl;

	const unsigned int sceneResWidth = usedResolution.x();
	const unsigned int sceneResHeight = usedResolution.y();

	HierarchyView::Ptr	pointBasedView(new HierarchyView(scene, sceneResWidth, sceneResHeight, toload, scaffold, myArgs.budget.get()));

	// Raycaster.
	std::shared_ptr<sibr::Raycaster> raycaster = std::make_shared<sibr::Raycaster>();
	raycaster->init();
	raycaster->addMesh(scene->proxies()->proxy());

	// Camera handler for main view.
	sibr::InteractiveCameraHandler::Ptr generalCamera(new InteractiveCameraHandler());
	generalCamera->setup(scene->cameras()->inputCameras(), Viewport(0, 0, (float)usedResolution.x(), (float)usedResolution.y()), raycaster, { -1.0f,-1.0f });

	// Add views to mvm.
	MultiViewManager        multiViewManager(window, false);

	if (myArgs.rendering_mode == 1) 
		multiViewManager.renderingMode(IRenderingMode::Ptr(new StereoAnaglyphRdrMode()));
	
	multiViewManager.addIBRSubView("Point view", pointBasedView, usedResolution, ImGuiWindowFlags_ResizeFromAnySide);
	multiViewManager.addCameraForView("Point view", generalCamera);

	// Top view
	const std::shared_ptr<sibr::SceneDebugView> topView(new sibr::SceneDebugView(scene, generalCamera, myArgs,  myArgs.imagesPath.get().c_str()));
	multiViewManager.addSubView("Top view", topView, usedResolution);
	topView->active(false);

	CHECK_GL_ERROR;

	// save images
	generalCamera->getCameraRecorder().setViewPath(pointBasedView, myArgs.dataset_path.get());
	if (myArgs.pathFile.get() !=  "" ) {
		generalCamera->getCameraRecorder().loadPath(myArgs.pathFile.get(), usedResolution.x(), usedResolution.y());
		generalCamera->getCameraRecorder().recordOfflinePath(myArgs.outPath, multiViewManager.getIBRSubView("Point view"), "");
		if( !myArgs.noExit )
			exit(0);
	}

	// Start UDP server
	std::thread udpServerThread;
    if (udpEnabled) {
        std::cout << "UDP Enabled! Starting UDP server..." << std::endl;
        
		_running = true;
		udpServerThread = std::thread(runUDPServer, std::ref(_running));

		// Enable JSON camera mode
		generalCamera->switchMode(sibr::InteractiveCameraHandler::JSON);
    }

	// Main looooooop.
	while (window.isOpened()) {

		sibr::Input::poll();
		window.makeContextCurrent();
		if (sibr::Input::global().key().isPressed(sibr::Key::Escape)) {
			window.close();
		}
		
		sibr::Input& input = sibr::Input::global();

		// Check if cameraTransform is available and update the camera
		if (_newData) {
			_newData =  false;
			std::lock_guard<std::mutex> lock(cameraTransformMutex);
			generalCamera->updateCameraTransform(cameraTransform->position, cameraTransform->rotation);
		}
		
		multiViewManager.onUpdate(input);
		multiViewManager.onRender(window);

		window.swapBuffer();
		CHECK_GL_ERROR;
	}

	// Clean up
    if (udpEnabled) {
        _running = false; // Stop the UDP server
        udpServerThread.join(); // Wait for the UDP server thread to finish
    }

	return EXIT_SUCCESS;
}
