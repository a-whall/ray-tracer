1. Clone the repository.

2. Update the COMPILER variable in the project Makefile.

### Windows

3. Download the required libraries for this project.
[SDL2 2.24.0](https://github.com/libsdl-org/SDL/releases/tag/release-2.24.0) | 
[SDL2_image 2.6.2](https://github.com/libsdl-org/SDL_image/releases/tag/release-2.6.2) | 
[glm-0.9.9.8](https://github.com/g-truc/glm/releases/tag/0.9.9.8)

4. Open the project Makefile and replace the value of LDIR with the name of the folder into which your library zip files were extracted.

5. Copy SDL2.dll and SDL2_image.dll to the projects root directory.

### Linux

3. Install the following packages: libsdl2-dev, libsdl2-image-dev, libglm-dev