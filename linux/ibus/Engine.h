#pragma once

#include <ibus.h>

// The IBus engine type ("july-bangla"). Typing follows the Windows text service: the shared
// engine decides the text; this layer only shows it as pre-edit text and commits it.
GType july_ibus_engine_get_type();

// Runs the engine on fixed key sequences without IBus (used by ctest). Returns 0 on success.
int julySelfTest();
