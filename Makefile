APPNAME = ray
COMPILER = # replace with desired C++ compiler
CPPSTD = c++17

HPP_FILES = application ray-tracer-app
CPP_FILES = application ray-tracer-app main

LDIR = # (windows only) replace as instructed in doc/setup.md
GLM_LDIR = $(LDIR)/glm-0.9.9.8/glm
SDL2_LDIR = $(LDIR)/SDL2-devel-2.24.0-VC/SDL2-2.24.0
SDL2_IMG_LDIR = $(LDIR)/SDL2_image-devel-2.6.2-VC/SDL2_image-2.6.2

WFLAGS = -Wpedantic -Wall -Wextra
LFLAGS = -lSDL2main -lSDL2 -lSDL2_image
IFLAGS = -Iinclude

ifeq ($(shell uname), Linux)
detected_os = linux
LFLAGS := -lOpenGL $(LFLAGS)
IFLAGS += -I/usr/include/glm -I/usr/include/SDL2 -I/usr/include/SDL2_image
APPBIN = $(APPNAME)
endif

ifeq ($(OS), Windows_NT)
detected_os = windows
LPATHS = -L$(SDL2_LDIR)/lib/x64 -L$(SDL2_IMG_LDIR)/lib/x64
IFLAGS += -I$(SDL2_LDIR)/include -I$(SDL2_IMG_LDIR)/include -I$(GLM_LDIR)/glm
APPBIN = $(APPNAME).exe
ifeq ($(LDIR), )
$(error LDIR variable is empty. Please see the windows section of doc/setup.md)
endif
endif

ifndef detected_os
$(error Failed to detect OS)
endif

ifeq ($(COMPILER), )
$(error COMPILER variable is empty)
endif

HEADERS = $(patsubst %,include/%.h,$(HPP_FILES))
OBJECTS = $(patsubst %,obj/%.o,$(CPP_FILES))

$(APPBIN): $(OBJECTS) obj/glad.o
	$(COMPILER) $^ $(WFLAGS) $(LPATHS) $(LFLAGS) -o $@

obj/glad.o: src/glad.cpp include/glad.h
	$(COMPILER) -c -std=$(CPPSTD) $< -Iinclude -o $@

$(OBJECTS): obj/%.o: src/%.cpp $(HEADERS)
	$(COMPILER) -c -std=$(CPPSTD) $< $(WFLAGS) $(IFLAGS) -g -o $@

glad: obj/glad.o

all: $(APPBIN)

.PHONY: clean
clean: # assumes an environment like LLVM or MinGW for windows that provides rm.exe
	rm -f obj/main.o
	rm -f obj/application.o
	rm -f obj/ray-tracer-app.o
	rm -f $(APPBIN)