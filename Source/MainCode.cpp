#include <iostream>         // error handling and output
#include <cstdlib>          // EXIT_FAILURE, EXIT_SUCCESS, exit()

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN // skip rarely used Windows headers
#define NOMINMAX            // keep Windows min/max macros away from GLM
#include <windows.h>        // GetModuleFileNameW, SetCurrentDirectoryW
#endif

#include <GL/glew.h>        // GLEW library
#include "GLFW/glfw3.h"     // GLFW library

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SceneManager.h"
#include "ViewManager.h"
#include "ShapeMeshes.h"
#include "ShaderManager.h"

// namespace for declaring global variables
namespace
{
	// title shown in the display window's title bar
	const char* const WINDOW_TITLE = "7-1 FinalProject and Milestones"; 

	// main GLFW window
	GLFWwindow* g_Window = nullptr;

	// scene manager object for managing the 3D scene prepare and render
	SceneManager* g_SceneManager = nullptr;
	// shader manager object for dynamic interaction with the shader code
	ShaderManager* g_ShaderManager = nullptr;
	// view manager object for managing the 3D view setup and projection to 2D
	ViewManager* g_ViewManager = nullptr;
}

// function declarations. These functions are defined after main(),
// so they must be declared before main() can call them
bool InitializeGLFW();
bool InitializeGLEW();
void SetWorkingDirectoryToProject();


/***********************************************************
 *  main(int, char*[])
 *
 *  This function gets called after the application has been
 *  launched.
 ***********************************************************/
int main(int argc, char* argv[])
{
	// start from the project folder so the relative shader and texture
	// paths resolve the same way on F5 and when the EXE is double-clicked
	SetWorkingDirectoryToProject();

	// terminate the application if InitializeGLFW() returns false. It
	// always returns true, since it does not check the result of glfwInit()
	if (InitializeGLFW() == false)
	{
		return(EXIT_FAILURE);
	}

	// try to create a new shader manager object
	g_ShaderManager = new ShaderManager();
	// try to create a new view manager object
	g_ViewManager = new ViewManager(
		g_ShaderManager);

	// try to create the main display window
	g_Window = g_ViewManager->CreateDisplayWindow(WINDOW_TITLE);

	// if GLEW fails initialization, then terminate the application
	if (InitializeGLEW() == false)
	{
		return(EXIT_FAILURE);
	}

	// load the shader code from the external GLSL files. This project
	// ships its own copies in its Shaders folder, which extend the course
	// lighting to what the scene uses, so the course shaders in Utilities
	// stay unchanged for the other projects that share them
	g_ShaderManager->LoadShaders(
		"Shaders/vertexShader.glsl",
		"Shaders/fragmentShader.glsl");
	g_ShaderManager->use();

	// try to create a new scene manager object and prepare the 3D scene
	g_SceneManager = new SceneManager(g_ShaderManager);
	g_SceneManager->PrepareScene();

	// keep running until the window is asked to close, for example by
	// its close button or by the Escape key
	while (!glfwWindowShouldClose(g_Window))
	{
		// enable z-depth
		glEnable(GL_DEPTH_TEST);

		// clear the frame and z buffers
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// update the frame timing, handle any held keys (which can close the
		// window, switch the view, move the camera, or zoom the orthographic
		// view), then set the view and projection matrices that convert the
		// 3D scene to the 2D window
		g_ViewManager->PrepareSceneView();

		// refresh the 3D scene
		g_SceneManager->RenderScene();


		// swap the back buffer with the front buffer every frame
		glfwSwapBuffers(g_Window);

		// process pending GLFW events, which runs the mouse, scroll, and
		// resize callbacks and updates the key states for the next frame
		glfwPollEvents();
	}

	// clear the allocated manager objects from memory
	if (NULL != g_SceneManager)
	{
		delete g_SceneManager;
		g_SceneManager = NULL;
	}
	if (NULL != g_ViewManager)
	{
		delete g_ViewManager;
		g_ViewManager = NULL;
	}
	if (NULL != g_ShaderManager)
	{
		delete g_ShaderManager;
		g_ShaderManager = NULL;
	}

	// terminate the program successfully
	exit(EXIT_SUCCESS); 
}

/***********************************************************
 *  InitializeGLFW()
 *
 *  This function is used to initialize the GLFW library.
 ***********************************************************/
bool InitializeGLFW()
{
	// GLFW: initialize and configure library
	// --------------------------------------
	// the result of glfwInit() is not checked, so this function
	// always returns true, even if GLFW fails to initialize
	glfwInit();

#ifdef __APPLE__
	// set the version of OpenGL and forward compatibility for macOS
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	// set the version of OpenGL and profile to use
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
	// GLFW: end -------------------------------

	return(true);
}

/***********************************************************
 *  InitializeGLEW()
 *
 *  This function is used to initialize the GLEW library.
 ***********************************************************/
bool InitializeGLEW()
{
	// GLEW: initialize
	// -----------------------------------------
	GLenum GLEWInitResult = GLEW_OK;

	// try to initialize the GLEW library
	GLEWInitResult = glewInit();
	if (GLEW_OK != GLEWInitResult)
	{
		std::cerr << glewGetErrorString(GLEWInitResult) << std::endl;
		return false;
	}
	// GLEW: end -------------------------------

	// display a successful OpenGL initialization message
	std::cout << "INFO: OpenGL Successfully Initialized\n";
	std::cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << "\n" << std::endl;

	return(true);
}

/***********************************************************
 *  SetWorkingDirectoryToProject()
 *
 *  This function is used to set the working directory to
 *  the project folder, which is the parent of the folder
 *  holding the EXE. Visual Studio already starts the app
 *  there on F5, but double-clicking the EXE starts it in
 *  the Debug (or Release) folder, where the "Shaders" paths
 *  passed to LoadShaders() and the "Textures" paths in
 *  SceneManager cannot be found.
 ***********************************************************/
void SetWorkingDirectoryToProject()
{
#ifdef _WIN32
	wchar_t exePath[MAX_PATH];

	// get the full EXE path, e.g. ...\Debug\7-1_FinalProjectMilestones.exe
	DWORD length = GetModuleFileNameW(NULL, exePath, MAX_PATH);
	if ((0 == length) || (MAX_PATH == length))
	{
		// path unavailable or truncated, so keep the current directory
		return;
	}

	// strip the EXE file name, then the Debug (or Release) folder, leaving
	// the project folder
	for (int i = 0; i < 2; i++)
	{
		wchar_t* lastSlash = wcsrchr(exePath, L'\\');
		if (nullptr == lastSlash)
		{
			return;
		}
		*lastSlash = L'\0';
	}

	// make the project folder the base for all relative file paths
	SetCurrentDirectoryW(exePath);
#endif
}