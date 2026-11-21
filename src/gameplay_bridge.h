#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
namespace gameplay_bridge {
inline constexpr size_t kEnemies=6;
inline constexpr const char* kNames[kEnemies]={"Granny","Grandpa","MomSpider","Spider","Crow","Rat"};
struct Enemy { int count=0;bool paused=false;float speed=1,animation=1,size=1;bool death_ready=false,clone_ready=false; };
struct Item { int id=0;std::string name; };
struct Snapshot {
    uint64_t context=0;bool ready=false,authority=false,spawn_ready=false,god_ready=false,god=false;
    std::array<Enemy,kEnemies> enemies{};std::vector<Item> items;
    std::string status=u8"Oyun sahnesi bekleniyor.";
    std::string spawn_status=u8"Maç envanteri bekleniyor.",god_status=u8"Maç karakteri bekleniyor.";
};
enum class Kind { EnemySettings, ResetEnemy, RemoveEnemy, KillEnemy, CloneEnemy, Spawn, God };
struct Command { Kind kind;uint64_t context;size_t enemy=0;Enemy settings{};int item=0;bool flag=false;double queued=0; };
inline std::mutex mutex;
inline Snapshot snapshot;
inline std::deque<Command> commands;
inline double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline Snapshot Read(){std::lock_guard<std::mutex> lock(mutex);return snapshot;}
inline void Publish(const Snapshot& value){std::lock_guard<std::mutex> lock(mutex);snapshot=value;}
inline bool Queue(Command value){
    std::lock_guard<std::mutex> lock(mutex);
    if(!snapshot.ready||!value.context||value.context!=snapshot.context)return false;
    if(value.enemy>=kEnemies)return false;
    if(value.kind==Kind::EnemySettings){
        if(!std::isfinite(value.settings.speed)||!std::isfinite(value.settings.animation)||!std::isfinite(value.settings.size))return false;
        value.settings.speed=std::clamp(value.settings.speed,.1f,5.f);
        value.settings.animation=std::clamp(value.settings.animation,.1f,3.f);
        value.settings.size=std::clamp(value.settings.size,.25f,value.enemy==4?20.f:3.f);
    }
    value.queued=Now();
    if(value.kind==Kind::EnemySettings){
        for(auto& old:commands)if(old.kind==value.kind&&old.context==value.context&&old.enemy==value.enemy){old=value;return true;}
    }
    if(commands.size()>=24)return false;
    commands.push_back(value);return true;
}
inline std::deque<Command> Take(){std::lock_guard<std::mutex> lock(mutex);std::deque<Command> result;result.swap(commands);return result;}
inline void Clear(){std::lock_guard<std::mutex> lock(mutex);commands.clear();}
}
