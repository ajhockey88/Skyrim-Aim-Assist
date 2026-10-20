#pragma once

// Precompiled for every source in the target, including the plugin-info file that
// add_commonlibsse_plugin generates. That file includes REL/Relocation.h first, which in
// commonlibsse-ng-flatrim does not pull in CommonLib's own base header, so it must come from here.
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

using namespace std::literals;
