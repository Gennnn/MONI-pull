// precompiling large headers reduces compile times
// it also makes intellisense less angry and confused

// the "heart" of our game runs in the EnTT ECS.
// documentation found here: https://skypjack.github.io/entt/
// suggested reading: https://skypjack.github.io/entt/md_docs_md_entity.html
#include "../entt-3.13.1/single_include/entt/entt.hpp"

// Gateware wraps talking to the O.S.(windows) and collision for us.
// documentation found here: gateware-main/documentation/html/index.html
#define GATEWARE_ENABLE_CORE
#define GATEWARE_ENABLE_MATH
#define GATEWARE_ENABLE_MATH2D
#define GATEWARE_ENABLE_INPUT
#define GATEWARE_ENABLE_SYSTEM
#define GATEWARE_ENABLE_GRAPHICS
#define GATEWARE_ENABLE_AUDIO
// disable some graphics libs we don't need
#define GATEWARE_DISABLE_GOPENGLSURFACE
#define GATEWARE_DISABLE_GRASTERSURFACE
#define GATEWARE_DISABLE_GDIRECTX11SURFACE
#define GATEWARE_DISABLE_GDIRECTX12SURFACE
#include "../gateware-main/Gateware.h"

// JSON parser for level importing and loading.
#include "../json-develop/single_include/nlohmann/json.hpp"

// Ini reader
#include "../inifile-cpp-master/include/inicpp.h"

#include "../TrueType/stb_truetype.h"


