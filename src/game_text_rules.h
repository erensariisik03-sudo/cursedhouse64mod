#pragma once
#include "il2cpp_runtime.h"
namespace game_text_rules {
struct Status { bool input_filter = false, display_filter = false, username_limit = false; };
// Called once from the existing Unity scripting thread, after a real frame.
Status Install(const il2cpp::Api& api);
}
