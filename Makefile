UNAME_S := $(shell uname -s)

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
CXXFLAGS += -Ivendor/imgui -Ivendor/imgui/backends -Ivendor/libalea/include
CXXFLAGS += $(shell sdl2-config --cflags)

ifeq ($(UNAME_S),Darwin)
CXXFLAGS += $(if $(shell which brew 2>/dev/null),-I$(shell brew --prefix)/include,)
LDFLAGS   = $(shell sdl2-config --libs) -framework OpenGL -ldl -lm -lpthread
else
CXXFLAGS += -fopenmp
LDFLAGS   = $(shell sdl2-config --libs) -lGL -ldl -lm -lpthread -fopenmp
endif

# libalea built from source (vendor/libalea submodule)
LIBALEA_DIR = vendor/libalea
LIBALEA_BIN = $(LIBALEA_DIR)/bin

CSG_LIBS = $(LIBALEA_BIN)/libalea_slice.a \
           $(LIBALEA_BIN)/libalea_raycast.a \
           $(LIBALEA_BIN)/libalea_mcnp.a \
           $(LIBALEA_BIN)/libalea_openmc.a \
           $(LIBALEA_BIN)/libalea.a

ifeq ($(UNAME_S),Darwin)
CSG_LINK = $(LIBALEA_BIN)/libalea_slice.a \
           $(LIBALEA_BIN)/libalea_raycast.a \
           -Wl,-force_load,$(LIBALEA_BIN)/libalea_mcnp.a \
           -Wl,-force_load,$(LIBALEA_BIN)/libalea_openmc.a \
           $(LIBALEA_BIN)/libalea.a
else
CSG_LINK = $(LIBALEA_BIN)/libalea_slice.a \
           $(LIBALEA_BIN)/libalea_raycast.a \
           -Wl,--whole-archive \
           $(LIBALEA_BIN)/libalea_mcnp.a \
           $(LIBALEA_BIN)/libalea_openmc.a \
           -Wl,--no-whole-archive \
           $(LIBALEA_BIN)/libalea.a
endif

IMGUI_DIR = vendor/imgui

# Application sources
APP_SRCS = src/main.cpp src/app.cpp src/panel_slice.cpp src/panel_3d.cpp src/panel_tree.cpp \
           src/panel_cli.cpp src/panel_status.cpp src/panel_properties.cpp src/commands.cpp \
           src/slice_worker.cpp src/raycast_worker.cpp

APP_OBJS = $(patsubst src/%.cpp,build/%.o,$(APP_SRCS))

# ImGui objects (explicit list to avoid patsubst issues with nested dirs)
IMGUI_OBJS = build/imgui.o build/imgui_demo.o build/imgui_draw.o \
             build/imgui_tables.o build/imgui_widgets.o \
             build/imgui_impl_sdl2.o build/imgui_impl_opengl3.o

TARGET = bin/aleaviewer

.PHONY: all clean libalea

all: $(TARGET)

# Build libalea from submodule source
libalea:
	$(MAKE) -C $(LIBALEA_DIR) lib-core modules RELEASE=1 USE_OPENMP=1

$(CSG_LIBS): libalea

$(TARGET): $(APP_OBJS) $(IMGUI_OBJS) $(CSG_LIBS)
	@mkdir -p bin
	$(CXX) -o $@ $(APP_OBJS) $(IMGUI_OBJS) $(CSG_LINK) $(LDFLAGS)

# Application objects
build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ImGui core objects
build/imgui.o: $(IMGUI_DIR)/imgui.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/imgui_demo.o: $(IMGUI_DIR)/imgui_demo.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/imgui_draw.o: $(IMGUI_DIR)/imgui_draw.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/imgui_tables.o: $(IMGUI_DIR)/imgui_tables.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/imgui_widgets.o: $(IMGUI_DIR)/imgui_widgets.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ImGui backend objects
build/imgui_impl_sdl2.o: $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/imgui_impl_opengl3.o: $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build bin

clean-libalea:
	$(MAKE) -C $(LIBALEA_DIR) clean
