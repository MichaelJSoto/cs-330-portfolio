///////////////////////////////////////////////////////////////////////////////
// viewmanager.h
// ============
// manage the viewing of 3D objects within the viewport
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ShaderManager.h"
#include "camera.h"

// GLFW library
#include "GLFW/glfw3.h"

class ViewManager
{
public:
	// constructor
	ViewManager(
		ShaderManager* pShaderManager);
	// destructor
	~ViewManager();

	// mouse position callback for mouse interaction with the 3D scene
	static void Mouse_Position_Callback(GLFWwindow* window, double xMousePos, double yMousePos);
	// mouse scroll wheel callback for adjusting the camera movement speed
	static void Mouse_Scroll_Wheel_Callback(GLFWwindow* window, double xOffset, double yOffset);
	// window resize callback for keeping the display matched to the window size
	static void Window_Resize_Callback(GLFWwindow* window, int width, int height);

private:
	// pointer to shader manager object
	ShaderManager* m_pShaderManager;
	// active OpenGL display window
	GLFWwindow* m_pWindow;

	// check the held keys each frame to close the window, switch the view,
	// move the camera, or zoom the orthographic view
	void ProcessKeyboardEvents();
	// switch between the perspective and orthographic views on the P and O keys
	void ProcessProjectionKeys();
	// move the camera, or zoom the orthographic view, on the movement keys
	void ProcessMovementKeys();
	// keep the camera above the desktop and near enough to see the scene
	void KeepCameraInBounds();

	// switch to the orthographic projection from the same viewpoint, sized
	// so the objects keep about the size they had
	void SetOrthographicView();
	// switch back to the perspective projection
	void SetPerspectiveView();

	// build the projection matrix for the 3D perspective view
	glm::mat4 GetPerspectiveProjection() const;
	// build the projection matrix for the 2D orthographic view
	glm::mat4 GetOrthographicProjection() const;

	// list the camera controls in the console window
	void PrintControls() const;

public:
	// create the initial OpenGL display window
	GLFWwindow* CreateDisplayWindow(const char* windowTitle);

	// update the frame timing, handle held keys, and set the view and
	// projection matrices that convert the 3D scene to the 2D window
	void PrepareSceneView();
};
