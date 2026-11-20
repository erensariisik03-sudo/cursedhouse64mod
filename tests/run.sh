#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src tests/text_buffer_test.cpp -o build/tests/text_buffer_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src tests/shortcut_test.cpp -o build/tests/shortcut_test
build/tests/shortcut_test
build/tests/text_buffer_test
"${CXX:-g++}" -std=c++17 -O0 -DIMGUI_ENABLE_TEST_ENGINE -DIMGUI_USE_WCHAR32 -I src -I third_party/imgui \
    tests/keyboard_ui_test.cpp src/keyboard_ui.cpp \
    third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp \
    third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp \
    -o build/tests/keyboard_ui_test
build/tests/keyboard_ui_test
"${CXX:-g++}" -std=c++17 -fPIC -shared -I src tests/fake_il2cpp.cpp -o build/tests/libil2cpp.so
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Wno-unused-parameter -I tests/stubs -I src -I include \
    tests/startup_test.cpp src/il2cpp_runtime.cpp src/emoji_support.cpp src/game_text_rules.cpp src/observer_target.cpp src/cosmetic_catalog.cpp src/cosmetic_room.cpp src/lobby_emotes.cpp src/chat_bot.cpp src/ad_block.cpp src/auto_join.cpp -ldl -pthread -o build/tests/startup_test
LD_LIBRARY_PATH="$PWD/build/tests${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" build/tests/startup_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -I tests/stubs -I src \
    tests/emoji_support_test.cpp src/emoji_support.cpp src/il2cpp_runtime.cpp -ldl -o build/tests/emoji_support_test
build/tests/emoji_support_test
"${CXX:-g++}" -std=c++17 -DDOBBY_LOGGING_DISABLE \
    -I third_party/dobby -I third_party/dobby/source -I third_party/dobby/source/Backend/UserMode \
    -I third_party/dobby/include -I third_party/dobby/external -I third_party/dobby/external/logging \
    tests/intercept_entry_test.cpp third_party/dobby/source/InterceptEntry.cpp -o build/tests/intercept_entry_test
build/tests/intercept_entry_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src \
    tests/username_gate_test.cpp -o build/tests/username_gate_test
build/tests/username_gate_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -I tests/stubs -I src \
    tests/game_text_rules_test.cpp src/game_text_rules.cpp src/il2cpp_runtime.cpp -ldl -o build/tests/game_text_rules_test
build/tests/game_text_rules_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Wno-unused-parameter -I tests/stubs -I src -I include \
    tests/unity_editor_test.cpp src/il2cpp_runtime.cpp src/emoji_support.cpp src/game_text_rules.cpp src/observer_target.cpp src/cosmetic_catalog.cpp src/cosmetic_room.cpp src/lobby_emotes.cpp src/chat_bot.cpp src/ad_block.cpp src/auto_join.cpp \
    -ldl -pthread -o build/tests/unity_editor_test
build/tests/unity_editor_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src \
    tests/hook_veneer_test.cpp -o build/tests/hook_veneer_test
build/tests/hook_veneer_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I tests/stubs -I src \
    tests/cosmetic_catalog_test.cpp src/cosmetic_catalog.cpp src/il2cpp_runtime.cpp -ldl -o build/tests/cosmetic_catalog_test
build/tests/cosmetic_catalog_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src \
    tests/cosmetic_room_test.cpp -o build/tests/cosmetic_room_test
build/tests/cosmetic_room_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I tests/stubs -I src \
    tests/cosmetic_room_runtime_test.cpp src/il2cpp_runtime.cpp -ldl -o build/tests/cosmetic_room_runtime_test
build/tests/cosmetic_room_runtime_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src \
    tests/arm64_native_test.cpp -o build/tests/arm64_native_test
build/tests/arm64_native_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I tests/stubs -I src \
    tests/lobby_emotes_test.cpp src/lobby_emotes.cpp src/il2cpp_runtime.cpp -ldl -o build/tests/lobby_emotes_test
build/tests/lobby_emotes_test
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src -pthread \
    tests/chat_bot_test.cpp -o build/tests/chat_bot_test
build/tests/chat_bot_test
