#include "voice_ai_bridge.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
int main(){
 using namespace myinstants;
 std::vector<Sound> favorites;
 Sound a={"Vine \"Boom\"", "/media/sounds/vine-boom.mp3"}, b={"Türkçe ses", "/media/sounds/bruh.mp3"};
 assert(ToggleFavorite(favorites,a)&&ToggleFavorite(favorites,b));
 assert(IsFavorite(favorites,a.path));
 std::istringstream in(SerializeFavorites(favorites));auto restored=ParseFavorites(in);
 assert(restored.size()==2&&restored[0].title==a.title&&restored[1].title==b.title);
 std::string path="/tmp/gmm_favorites_test_"+std::to_string(getpid());assert(SaveFavorites(path,favorites));
 std::ifstream file(path);assert(ParseFavorites(file).size()==2);std::remove(path.c_str());
 assert(ToggleFavorite(restored,a)&&!IsFavorite(restored,a.path));
 assert(!ToggleFavorite(restored,{"bad","https://bad.com/a.mp3"}));
 voice_ai_bridge::desired.enabled=false;voice_ai_bridge::soundboard.favorites=restored;
 voice_ai_bridge::PlaySound(b.path);assert(voice_ai_bridge::soundboard.play_pending==b.path);
 std::cout<<"Favorites save/reload/remove and playback request with AI off: PASS\n";
}
