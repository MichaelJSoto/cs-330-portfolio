///////////////////////////////////////////////////////////////////////////////
// viewmanager.cpp
// ============
// manage the viewing of 3D objects within the viewport
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "ViewManager.h"

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// declaration of the global variables and constants
namespace
{
	// starting width and height of the display window, in pixels
	const int WINDOW_WIDTH = 1000;
	const int WINDOW_HEIGHT = 800;
	// names of the view and projection matrix uniforms in the vertex shader
	const char* g_ViewName = "view";
	const char* g_ProjectionName = "projection";

	// camera object used for viewing and interacting with
	// the 3D scene
	Camera* g_pCamera = nullptr;

	// these variables are used for mouse movement processing
	float gLastX = WINDOW_WIDTH / 2.0f;
	float gLastY = WINDOW_HEIGHT / 2.0f;
	bool gFirstMouse = true;

	// seconds between the current frame and the last frame, and the
	// glfwGetTime() value recorded on the last frame
	float gDeltaTime = 0.0f;
	float gLastFrame = 0.0f;

	// width divided by height of the display window, updated by
	// Window_Resize_Callback() so the projection never stretches the scene
	float gAspectRatio = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);

	// degrees the camera turns for each pixel of mouse travel. camera.h
	// defaults to 0.01, which turns the camera only 10 degrees across the
	// full 1000 pixel window width, so the LearnOpenGL value of 0.1 is used
	const float MOUSE_SENSITIVITY = 0.1f;

	// the starting camera stands where IMG_0055 was taken, solved from the
	// desk's traced edges in that photo. The photo is also rolled 0.91
	// degrees, which a camera turned only by yaw and pitch cannot show
	const glm::vec3 START_POSITION = glm::vec3(0.68f, 15.62f, 26.16f);
	const float START_YAW = -90.59f;
	const float START_PITCH = -25.50f;
	// the photo's view is 68.9 degrees across, and this vertical field of
	// view shows the same width in the 1000 by 800 window
	const float FIELD_OF_VIEW = 57.6f;

	// the starting movement speed, in units per second, crosses the 28.12
	// unit desk in about 4 seconds
	const float DEFAULT_SPEED = 7.0f;
	// each notch of the scroll wheel multiplies or divides the speed by
	// SPEED_STEP, so every notch changes the speed by the same factor at any
	// speed, within these limits
	const float SPEED_STEP = 1.25f;
	const float MIN_SPEED = 0.5f;
	const float MAX_SPEED = 40.0f;
	// while Left Shift is held, the camera moves at this share of its speed,
	// for placing it precisely
	const float PRECISION_FACTOR = 0.25f;
	// true while Left Shift is held, so a change can be reported once
	bool gPrecisionMovement = false;

	// the closest and farthest distances from the camera that are drawn,
	// shared by both projections
	const float NEAR_PLANE = 0.1f;
	const float FAR_PLANE = 100.0f;

	// a sphere that holds the desk and every object on it: every desk
	// corner, the books' overhang, and the top of the lamp lie within 17.43
	// units of this center. Keeping the camera within MAX_CAMERA_DISTANCE of
	// it keeps them all closer than the far plane (80 + 17.5 < 100)
	const glm::vec3 SCENE_CENTER = glm::vec3(0.0f, 6.0f, 0.0f);
	const float MAX_CAMERA_DISTANCE = 80.0f;

	// the lowest height the camera can reach, so it stops just above the
	// desktop at y = 0 instead of passing through it. In the perspective
	// view in the starting 1000 by 800 window every point of the near plane
	// lies within 0.14 units of the camera, so at this height that near
	// plane cannot cut into the desk at any tilt
	const float MIN_CAMERA_HEIGHT = 0.25f;

	// the following variable is false when orthographic projection
	// is off and true when it is on
	bool bOrthographicProjection = false;

	// the middle of the desktop, the scene's origin, whose depth sizes the
	// orthographic view
	const glm::vec3 DESK_MIDDLE = glm::vec3(0.0f, 0.0f, 0.0f);

	// half the height of the scene area shown in the orthographic view, in
	// world units, set when the view is switched on and changed by the W and
	// S keys. The limits run from a 1 unit tall close-up to a 100 unit tall
	// view. Switched on from the farthest place the camera can reach, 86
	// units from the desk's middle, the view is at most 94.6 units tall
	const float ORTHO_MIN_HALF_HEIGHT = 0.5f;
	const float ORTHO_MAX_HALF_HEIGHT = 50.0f;
	float gOrthoHalfHeight = ORTHO_MIN_HALF_HEIGHT;
}

/***********************************************************
 *  ViewManager()
 *
 *  The constructor for the class.
 ***********************************************************/
ViewManager::ViewManager(
	ShaderManager *pShaderManager)
{
	// initialize the member variables
	m_pShaderManager = pShaderManager;
	m_pWindow = NULL;
	g_pCamera = new Camera();
	// default camera view parameters: the camera's Zoom value is its
	// vertical field of view
	g_pCamera->Position = START_POSITION;
	g_pCamera->Zoom = FIELD_OF_VIEW;

	// aim the camera the way IMG_0055 looks at the desk. The camera rebuilds
	// its Front vector from the Yaw and Pitch angles on every mouse movement,
	// so the angles are set rather than Front. Setting Front directly would
	// leave Pitch at 0 and snap the view level on the first mouse movement
	g_pCamera->Yaw = START_YAW;
	g_pCamera->Pitch = START_PITCH;

	// a zero offset leaves Yaw and Pitch unchanged, but makes the camera
	// recalculate its Front, Right, and Up vectors from them
	g_pCamera->ProcessMouseMovement(0.0f, 0.0f);

	// set how far the camera turns for each pixel of mouse travel, and how
	// fast it moves
	g_pCamera->MouseSensitivity = MOUSE_SENSITIVITY;
	g_pCamera->MovementSpeed = DEFAULT_SPEED;
}

/***********************************************************
 *  ~ViewManager()
 *
 *  The destructor for the class.
 ***********************************************************/
ViewManager::~ViewManager()
{
	// clear the pointers and free the camera object. The shader
	// manager is not deleted here, since main() created it and
	// deletes it
	m_pShaderManager = NULL;
	m_pWindow = NULL;
	if (NULL != g_pCamera)
	{
		delete g_pCamera;
		g_pCamera = NULL;
	}
}

/***********************************************************
 *  CreateDisplayWindow()
 *
 *  This method is used to create the main display window.
 ***********************************************************/
GLFWwindow* ViewManager::CreateDisplayWindow(const char* windowTitle)
{
	GLFWwindow* window = nullptr;

	// try to create the displayed OpenGL window
	window = glfwCreateWindow(
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		windowTitle,
		NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return NULL;
	}
	glfwMakeContextCurrent(window);

	// this callback is used to receive window sizing events
	glfwSetFramebufferSizeCallback(window, &ViewManager::Window_Resize_Callback);
	// this callback is used to receive mouse moving events
	glfwSetCursorPosCallback(window, &ViewManager::Mouse_Position_Callback);
	// this callback is used to receive mouse wheel scrolling events
	glfwSetScrollCallback(window, &ViewManager::Mouse_Scroll_Wheel_Callback);

	// tell GLFW to capture all mouse events. The cursor is hidden and
	// held inside the window, so the camera can keep turning after the
	// mouse would have reached the edge of the window. Press the Escape
	// key to close the window
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// enable blending for transparent drawing. Only the glass desktop is
	// drawn see-through, and RenderDeskSurface() turns blending on just for it
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_pWindow = window;

	// the cursor is hidden once the window opens, so show the user how to
	// drive the camera in the console window
	PrintControls();

	return(window);
}

/***********************************************************
 *  Mouse_Position_Callback()
 *
 *  This method is automatically called from GLFW whenever
 *  the mouse is moved within the active GLFW display window.
 ***********************************************************/
void ViewManager::Mouse_Position_Callback(GLFWwindow* window, double xMousePos, double yMousePos)
{
	// GLFW reports the mouse position as double values, while the
	// camera works in float values
	float xPosition = static_cast<float>(xMousePos);
	float yPosition = static_cast<float>(yMousePos);

	// when the first mouse move event is received, this needs to be
	// recorded so that all subsequent mouse moves can correctly calculate
	// the X position offset and Y position offset for proper operation
	if (gFirstMouse)
	{
		gLastX = xPosition;
		gLastY = yPosition;
		gFirstMouse = false;
	}

	// calculate the X offset and Y offset values for moving the 3D camera
	// accordingly. GLFW measures y from the top of the window down, so the
	// Y offset is reversed to make moving the mouse up give a positive offset
	float xOffset = xPosition - gLastX;
	float yOffset = gLastY - yPosition;

	// set the current positions into the last position variables
	gLastX = xPosition;
	gLastY = yPosition;

	// turn the camera according to the calculated offsets, in either view.
	// A positive X offset turns the camera right and a positive Y offset
	// tilts it up. The camera stops the tilt at 89 degrees up or down so the
	// view never flips over
	g_pCamera->ProcessMouseMovement(xOffset, yOffset);
}

/***********************************************************
 *  Mouse_Scroll_Wheel_Callback()
 *
 *  This method is automatically called from GLFW whenever
 *  the mouse scroll wheel is moved within the active GLFW
 *  display window. It changes how fast the camera travels
 *  while the movement keys are held down, and how fast the
 *  orthographic view zooms.
 ***********************************************************/
void ViewManager::Mouse_Scroll_Wheel_Callback(GLFWwindow* window, double xOffset, double yOffset)
{
	// if the camera object is null, then exit this method
	if (NULL == g_pCamera)
	{
		return;
	}

	// yOffset is positive when the wheel rolls forward (up) and negative
	// when it rolls back (down), 1.0 per notch on a standard wheel. Each
	// notch up multiplies the speed by SPEED_STEP and each notch down
	// divides it, so slow speeds change in small steps and fast speeds in
	// large ones. The camera's own ProcessMouseScroll() adds or subtracts a
	// whole unit per notch instead, which is too coarse at slow speeds
	float speed = g_pCamera->MovementSpeed * glm::pow(SPEED_STEP, static_cast<float>(yOffset));
	g_pCamera->MovementSpeed = glm::clamp(speed, MIN_SPEED, MAX_SPEED);

	// report the new speed in the console window, since a speed change
	// is not visible until the camera moves
	std::cout << "INFO: Camera movement speed: " << g_pCamera->MovementSpeed
		<< " units per second" << std::endl;
}

/***********************************************************
 *  Window_Resize_Callback()
 *
 *  This method is automatically called from GLFW whenever
 *  the display window is resized, maximized, or minimized.
 *  It keeps the drawing area and the projection aspect
 *  ratio matched to the new window size.
 ***********************************************************/
void ViewManager::Window_Resize_Callback(GLFWwindow* window, int width, int height)
{
	// a minimized window reports a size of zero, which would make the
	// aspect ratio invalid, so keep the last valid size until the window
	// is restored
	if ((width <= 0) || (height <= 0))
	{
		return;
	}

	// draw into the whole window instead of only the original 1000x800 area
	glViewport(0, 0, width, height);

	// save the new aspect ratio so the projection does not stretch the scene
	gAspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

/***********************************************************
 *  ProcessKeyboardEvents()
 *
 *  This method is called once per frame to check which keys
 *  are held down. It closes the window for Escape, switches
 *  between the perspective and orthographic views for P and
 *  O, and moves the camera for W, A, S, D, Q, and E (in the
 *  orthographic view, W and S zoom instead), more slowly
 *  while Left Shift is held. glfwGetKey() reads the key
 *  states that GLFW saved during the last glfwPollEvents()
 *  call.
 ***********************************************************/
void ViewManager::ProcessKeyboardEvents()
{
	// close the window if the escape key has been pressed
	if (glfwGetKey(m_pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(m_pWindow, true);
	}

	// if the camera object is null, then exit this method
	if (NULL == g_pCamera)
	{
		return;
	}

	// the projection is switched first, so any movement key held in the
	// same frame follows the rules of the view that is now shown
	ProcessProjectionKeys();
	ProcessMovementKeys();
}

/***********************************************************
 *  ProcessProjectionKeys()
 *
 *  This method is called to switch the display between the
 *  perspective (3D) view on the P key and the orthographic
 *  (2D) view on the O key.
 ***********************************************************/
void ViewManager::ProcessProjectionKeys()
{
	bool bOrthographicKey = (glfwGetKey(m_pWindow, GLFW_KEY_O) == GLFW_PRESS);
	bool bPerspectiveKey = (glfwGetKey(m_pWindow, GLFW_KEY_P) == GLFW_PRESS);

	// holding both keys asks for both views at once, so neither is applied.
	// Otherwise the view would flip between them on every frame
	if (bOrthographicKey && bPerspectiveKey)
	{
		return;
	}

	// the keys are checked on every frame, so a view is switched only when
	// it is not already shown. Holding a key down then switches once,
	// instead of resizing the view and printing a message on every frame
	if (bOrthographicKey && !bOrthographicProjection)
	{
		SetOrthographicView();
	}
	else if (bPerspectiveKey && bOrthographicProjection)
	{
		SetPerspectiveView();
	}
}

/***********************************************************
 *  ProcessMovementKeys()
 *
 *  This method is called to move the camera while the W, A,
 *  S, D, Q, and E keys are held down. In the orthographic
 *  view, W and S zoom the view in and out instead. Holding
 *  Left Shift slows every move and zoom to PRECISION_FACTOR
 *  of the speed.
 ***********************************************************/
void ViewManager::ProcessMovementKeys()
{
	// each key is checked on every frame so a held key moves the camera
	// continuously. Every move is scaled by the seconds since the last
	// frame, so the camera travels at the same speed at any frame rate.
	// Moves follow the camera's own directions, so forward always means
	// the direction the camera is facing

	// Left Shift scales the frame's time, so every move and zoom slows by
	// the same share. A change is reported once, when the key goes down or
	// up, since it only shows once the camera moves
	bool bPrecision = (glfwGetKey(m_pWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
	if (bPrecision != gPrecisionMovement)
	{
		gPrecisionMovement = bPrecision;
		std::cout << "INFO: Precision movement " << (bPrecision ? "on" : "off")
			<< ": " << g_pCamera->MovementSpeed * (bPrecision ? PRECISION_FACTOR : 1.0f)
			<< " units per second" << std::endl;
	}
	float moveTime = bPrecision ? gDeltaTime * PRECISION_FACTOR : gDeltaTime;

	// the distance the camera travels this frame, which is also how much
	// the orthographic view zooms, so the scroll wheel speed sets both
	float frameDistance = g_pCamera->MovementSpeed * moveTime;

	// process camera moving forward and backward. The orthographic view
	// draws objects the same size at any distance, so moving forward
	// would not make them look any larger. W and S shrink and grow the
	// area shown instead, which zooms in and out
	if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
	{
		if (bOrthographicProjection)
		{
			// zoom in by showing a smaller area of the scene
			gOrthoHalfHeight -= frameDistance;
		}
		else
		{
			// move the camera forward, toward what it is facing
			g_pCamera->ProcessKeyboard(FORWARD, moveTime);
		}
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
	{
		if (bOrthographicProjection)
		{
			// zoom out by showing a larger area of the scene
			gOrthoHalfHeight += frameDistance;
		}
		else
		{
			// move the camera backward, away from what it is facing
			g_pCamera->ProcessKeyboard(BACKWARD, moveTime);
		}
	}

	// keep the orthographic zoom within its limits
	gOrthoHalfHeight = glm::clamp(gOrthoHalfHeight, ORTHO_MIN_HALF_HEIGHT, ORTHO_MAX_HALF_HEIGHT);

	// process camera panning left and right. In the orthographic view these
	// keys and Q and E slide the flat view across the window
	if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
	{
		// pan the camera to the left
		g_pCamera->ProcessKeyboard(LEFT, moveTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
	{
		// pan the camera to the right
		g_pCamera->ProcessKeyboard(RIGHT, moveTime);
	}

	// process camera moving up and down
	if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
	{
		// move the camera up along its own up direction
		g_pCamera->ProcessKeyboard(UP, moveTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
	{
		// move the camera down, opposite its own up direction
		g_pCamera->ProcessKeyboard(DOWN, moveTime);
	}

	// keep the camera above the desktop and close enough that the far
	// plane never cuts off the desk or its objects
	KeepCameraInBounds();
}

/***********************************************************
 *  KeepCameraInBounds()
 *
 *  This method is called after the frame's moves to keep
 *  the camera above the desktop and close enough to the
 *  desk that the far plane cuts none of the desk or its
 *  objects, in both views.
 ***********************************************************/
void ViewManager::KeepCameraInBounds()
{
	// pull the camera back toward the scene's center if it has moved
	// farther away than MAX_CAMERA_DISTANCE
	glm::vec3 fromCenter = g_pCamera->Position - SCENE_CENTER;
	float distance = glm::length(fromCenter);
	if (distance > MAX_CAMERA_DISTANCE)
	{
		g_pCamera->Position = SCENE_CENTER + fromCenter * (MAX_CAMERA_DISTANCE / distance);
	}

	// the desktop acts as a floor, so the camera cannot sink through the
	// glass and look at the scene from underneath
	if (g_pCamera->Position.y < MIN_CAMERA_HEIGHT)
	{
		g_pCamera->Position.y = MIN_CAMERA_HEIGHT;
	}
}

/***********************************************************
 *  SetOrthographicView()
 *
 *  This method is called to switch to the orthographic view.
 *  The camera stays where it is and keeps facing the same
 *  way, so only the projection changes. With the desk's
 *  middle at least 0.91 units ahead, the area shown matches
 *  the perspective view at its depth, so the objects around
 *  it keep their size.
 ***********************************************************/
void ViewManager::SetOrthographicView()
{
	// how far ahead of the camera the desk's middle lies, along the way the
	// camera faces, or its straight distance if it is not ahead. At that
	// depth the perspective view shows depth * tan(FOV / 2) above and below
	// the middle of the window, so the orthographic view shows the same.
	// Nearer objects then look a little smaller and farther ones a little
	// larger, since an orthographic view draws everything at one scale
	glm::vec3 toDeskMiddle = DESK_MIDDLE - g_pCamera->Position;
	float depth = glm::dot(toDeskMiddle, g_pCamera->Front);
	if (depth <= 0.0f)
	{
		depth = glm::length(toDeskMiddle);
	}
	float halfHeight = depth * glm::tan(glm::radians(g_pCamera->Zoom) / 2.0f);
	gOrthoHalfHeight = glm::clamp(halfHeight, ORTHO_MIN_HALF_HEIGHT, ORTHO_MAX_HALF_HEIGHT);
	bOrthographicProjection = true;

	std::cout << "INFO: Orthographic (2D) view from the same place, "
		<< 2.0f * gOrthoHalfHeight << " units tall. "
		<< "Press P for the perspective view." << std::endl;
}

/***********************************************************
 *  SetPerspectiveView()
 *
 *  This method is called to switch back to the perspective
 *  view from wherever the camera now stands.
 ***********************************************************/
void ViewManager::SetPerspectiveView()
{
	// the camera itself was never moved for the orthographic view, so only
	// the projection changes back
	bOrthographicProjection = false;

	std::cout << "INFO: Perspective (3D) view. "
		<< "Press O for the orthographic view." << std::endl;
}

/***********************************************************
 *  GetPerspectiveProjection()
 *
 *  This method is used to build the projection matrix for
 *  the perspective view, where distant objects look smaller.
 ***********************************************************/
glm::mat4 ViewManager::GetPerspectiveProjection() const
{
	// the camera's Zoom value is the vertical field of view in degrees, and
	// the aspect ratio of the current window keeps the scene from stretching
	return(glm::perspective(glm::radians(g_pCamera->Zoom), gAspectRatio, NEAR_PLANE, FAR_PLANE));
}

/***********************************************************
 *  GetOrthographicProjection()
 *
 *  This method is used to build the projection matrix for
 *  the orthographic view, where every object keeps its size
 *  at any distance, like a flat 2D drawing.
 ***********************************************************/
glm::mat4 ViewManager::GetOrthographicProjection() const
{
	// the area shown is centered on the camera. Its half width is the half
	// height times the window's aspect ratio, so the scene keeps its true
	// proportions in any window shape
	float halfWidth = gOrthoHalfHeight * gAspectRatio;
	return(glm::ortho(-halfWidth, halfWidth, -gOrthoHalfHeight, gOrthoHalfHeight, NEAR_PLANE, FAR_PLANE));
}

/***********************************************************
 *  PrintControls()
 *
 *  This method is used to list the camera controls in the
 *  console window when the program starts.
 ***********************************************************/
void ViewManager::PrintControls() const
{
	std::cout << "INFO: Camera controls" << std::endl
		<< "  W / S          move forward / backward (orthographic view: zoom in / out)" << std::endl
		<< "  A / D          move left / right" << std::endl
		<< "  Q / E          move up / down" << std::endl
		<< "  Left Shift     hold to move at a quarter of the speed" << std::endl
		<< "  Mouse          look around" << std::endl
		<< "  Scroll wheel   raise / lower the movement speed, 1.25 times per notch" << std::endl
		<< "  P / O          perspective (3D) / orthographic (2D) view from the same place" << std::endl
		<< "  Esc            close the window" << std::endl
		<< "Movement speed: " << g_pCamera->MovementSpeed << " units per second" << std::endl << std::endl;
}

/***********************************************************
 *  PrepareSceneView()
 *
 *  This method is called once per frame, before the 3D scene
 *  is rendered. It updates the frame timing, applies any
 *  held keys to the camera, and passes the camera's view and
 *  projection matrices into the shader.
 ***********************************************************/
void ViewManager::PrepareSceneView()
{
	glm::mat4 view;
	glm::mat4 projection;

	// per-frame timing
	float currentFrame = static_cast<float>(glfwGetTime());
	gDeltaTime = currentFrame - gLastFrame;
	gLastFrame = currentFrame;

	// check the keys that are held down, which close the window,
	// switch between the perspective and orthographic views, and
	// move the camera or zoom the orthographic view
	ProcessKeyboardEvents();

	// get the current view matrix from the camera
	view = g_pCamera->GetViewMatrix();

	// get the projection matrix for the view that is currently chosen
	if (bOrthographicProjection)
	{
		projection = GetOrthographicProjection();
	}
	else
	{
		projection = GetPerspectiveProjection();
	}

	// if the shader manager object is valid
	if (NULL != m_pShaderManager)
	{
		// set the view matrix into the shader for proper rendering
		m_pShaderManager->setMat4Value(g_ViewName, view);
		// set the projection matrix into the shader for proper rendering
		m_pShaderManager->setMat4Value(g_ProjectionName, projection);
		// set the camera's view position into the shader for proper rendering
		m_pShaderManager->setVec3Value("viewPosition", g_pCamera->Position);
	}
}
