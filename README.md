# CS 330 Portfolio: Computational Graphics and Visualization

This repository is my portfolio submission for CS 330: Computational Graphics and Visualization. It holds my final project, a 3D scene of my own glass-top desk written in C++ with OpenGL. I photographed the desk for the Milestone One proposal and rebuilt it from those photographs: a closed laptop, two hardcover books with a ball tin on top, a tumbler, a mug with a pen in it, and a gooseneck lamp, all standing on black glass that reflects them.

![The desk scene as the program draws it](images/CS-330%20Final%20Project%20Scene.png)

Each object is built from the course's basic shapes, and with the glass plane the scene uses six of them. No object passes 1,000 triangles. The course's half torus alone drew 1,050, too many for a mug handle, so I wrote a small `BentTubeMesh` class that bends a tube along an arc and builds both the mug handle and the lamp's gooseneck. All 16 textures were made for this project at 1024 pixels or more per side. Four lights share the scene: the lamp as a 2700 K spotlight, two directional window lights, and a dim point light standing in for the wall's bounce. The shader adds all three Phong terms (ambient, diffuse, and specular) for each light.

## Repository Contents

| File | Description |
|---|---|
| `7-1_FinalProjectMilestones.zip` | The 3D Scene ZIP folder from the final project: the Visual Studio project files, the C++ source, the shaders, the 16 textures, and a built EXE in `Debug/`. |
| `CS-330 Final Project Design Decisions.docx` | The Design Decisions document from the final project. It justifies the development choices, explains how to navigate the scene, and describes the custom functions. |
| `CS-330 Final Project Design Decisions.pdf` | The same document as a PDF, readable in the browser. |
| `Source/` | The four C++ source files and three headers, copied unchanged from the zip so the code reads here without downloading anything. `SceneManager.cpp` builds and lights the scene, `ViewManager.cpp` handles the camera and input, `BentTubeMesh.cpp` builds the bent tube, and `MainCode.cpp` starts the program. |
| `Shaders/` | The project's vertex and fragment shaders, also copied unchanged from the zip. The fragment shader holds the lighting, the texture mappings, and the glass's Fresnel reflection. |
| `images/` | The screenshot shown above. |

## Running the Scene

The quickest way is the EXE inside the zip. Extract the zip to a folder and run `Debug/7-1_FinalProjectMilestones.exe`. At startup the program makes its project folder the working directory, so the shaders and textures load even when the EXE is started from Explorer. It needs Windows and a graphics driver that supports OpenGL 4.6.

To build from source, extract the zip into a new folder inside the `Projects` folder of the course's CS330Content package. The project reads GLFW, GLEW, GLM, the shader manager, and the shape meshes from `..\..\Libraries`, `..\..\Utilities`, and `..\..\3DShapes`. Then open `7-1_FinalProjectMilestones.sln` in Visual Studio 2022 and build.

The console lists the controls at launch, since the cursor is captured and hidden:

| Input | Action |
|---|---|
| W, A, S, D | Move forward, left, back, and right |
| Q, E | Move up and down along the camera's own up direction |
| Mouse | Look around at 0.1 degree per pixel. The tilt stops at 89 degrees, so the view never flips. |
| Scroll wheel | Change the speed by a factor of 1.25 per notch, from 0.5 to 40 units per second, starting at 7 |
| Left Shift (hold) | Move at a quarter of the speed |
| O, P | Switch to the orthographic or the perspective view without moving the camera. In the orthographic view, W and S zoom. |
| Esc | Close the window |

## Reflection

### How do I approach designing software?

The biggest design skill this project taught me was to design from measurement instead of from a glance. My scene is a real desk, so "close enough" had a real answer. I solved the camera from the desk's traced edges in three photographs and fitted each object to its outline, with the mug, 4 units across, as the scale. Once the photograph's 0.91 degree roll is removed, the launch view puts the laptop's edges and center within 2 pixels of the reference photograph, and the books, tumbler, and mug within 9. The same habit settled design questions later. When the course's half torus came to 1,050 triangles on its own, more than the 1,000 allowed for a whole object, I designed a replacement instead of trimming the rest of the mug. `BentTubeMesh` builds the handle's arc in 384 triangles with exact normals, and the same class bends the lamp's gooseneck.

My process followed the milestones, one layer at a time on top of a scene I fixed early. The Milestone One proposal committed me to five objects from my own photographs, and every later milestone built on that choice instead of starting over: shapes, then the camera, then textures, then lights, then the full desk. Within each layer I wrote down the options before choosing. Table 1 in the design document compares the approach I considered with the one I chose for nine parts of the scene, from the lamp beam to the reflection, each with what was measured or computed to decide it.

Two tactics will travel with me. The first is separating where something goes from how it is built. `SetObjectPlacement()` stores one matrix that moves every part drawn after it, so builders such as `RenderDeskLamp()` work in their own coordinates and can stand anywhere on the desk. `RenderDeskObjects()` draws every object twice per frame, once mirrored for the reflection and once upright, so the reflection needs no second copy of the drawing code. The second tactic is keeping the numbers that define an object in named constants at the top of the file, which is what let one `RenderBook()` function draw both books from a `BOOK_SPEC`. I also know where I fell short. The white plastic cylinder with a mapped flat end is written out three times, for the tumbler's lid, the lamp's base, and the shade's neck, and a `DrawCappedCylinder()` helper should have held it once. Spotting that kind of repetition sooner is the next design habit I want to build.

### How do I approach developing programs?

The new strategy that changed the most was testing a graphics program by measuring what it draws. Before this course, checking a picture meant looking at it. For this project I rendered test frames and counted pixels. Over 22 test views, the worst object's share of pixels with a color channel at 255 fell from 56 percent under Milestone Five's lights to 0.17 percent under the re-tuned ones. For the camera, I wrote a test harness that launches the real program, sends it keystrokes, mouse movement, and scroll-wheel notches, and measures where the scene moves on screen. It started with 26 tests in Milestone Three and passed all 28 on the final build. I also learned to read the course's mesh data instead of trusting it. In Milestone Four I found that the cylinder's side lists every triangle twice, once in each winding, which made a blended overlay composite twice until I added a stencil guard. In the final project I found that the stored texture coordinates stretched or skewed images on the caps, the tapered sides, and the ball tin, so the fragment shader now computes its own.

Iteration was built into the course, since each milestone's zip became the next one's starting point, but it mattered most inside the final project. Four of my approaches failed at first and were replaced. The shade's own cone lit the desk's middle with no visible edge, so the lamp became a narrower spotlight. Milestone Five's lights, tuned for the mug alone, washed out the laptop once the whole desk was in place, so I re-tuned all four. The course's texture coordinates stretched images, so the shader took over the mapping. The reflection left ten bright specks where objects touch the glass, because the mirrored underside of each object resting on the glass landed within 0.0004 units of the glass surface, closer than the depth buffer can separate at about 30 units from the camera. One line in the fragment shader fixed it by discarding mirrored fragments less than 0.001 units below the glass. The specks went to zero, and not one glass pixel was lost.

My approach changed across the milestones from "does it look right" to "can I show that it is right." In Milestone Two the scene was a dark gray desk and a mug drawn in flat color. Milestone Three added the camera and the first automated tests. Milestone Four is where I stopped trusting the starter code and started reading it: I fixed texture-loading defects in the course's own code and found the first of the mesh problems. Milestone Five moved the shaders into the project so I could change the lighting model itself. By the final project I was checking even the comments. I audited all 554 comment blocks in the nine source and shader files, fixed 69 of them, and confirmed that the rebuilt program's machine code did not change. That habit of proving a change did only what I meant it to do is what carried the project to completion.

### How can computer science help me in reaching my goals?

For my education, computational graphics turned math I had only met on paper into something I had to make work. Matrices, the perspective frustum, and depth precision stopped being formulas once they had to behave on screen. The formula behind my O key, where the view's half height equals the depth times tan(FOV / 2), came from working out how to keep objects at the desk's depth the same size when the projection switches. I plan to finish my degree in December, and the habits from this project carry into every course I have left, graphics or not: measure the output, and write down the alternatives I rejected and why.

For my career, the skills point in two directions. I work in security at ESPN, and in Module One I imagined a 3D model of the campus showing every camera and what each one covers. Now I could sketch the code. A security camera's view is a frustum, the same shape the perspective matrix builds, and the O key's formula gives a camera's coverage at any distance. Its blind spots are the shadow problem in disguise: put a light where the camera hangs, and whatever falls into shadow is what that camera cannot see. Broadcast graphics is the other direction. The yellow first-down line is, at its core, a virtual camera matched to a real one, and I built the slow, by-hand version of that when I solved my camera from three photographs. I am aiming to move into software engineering, ideally through an internal transfer at ESPN or Disney. One course does not qualify me for a graphics team, but I can now recognize the problems such a team solves every day, and I know how I would start on them.

## Contact

Michael Soto  
CS-330: Computational Graphics and Visualization  
Southern New Hampshire University
