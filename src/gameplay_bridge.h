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
#include "room_vote.h"
namespace gameplay_bridge {
inline constexpr size_t kEnemies=6;
inline constexpr const char* kNames[kEnemies]={"Granny","Grandpa","MomSpider","Spider","Crow","Rat"};
struct Enemy { int count=0;bool paused=false;float speed=1,animation=1,size=1;bool death_ready=false,clone_ready=false,pause_ready=false; };
struct Item { int id=0;std::string name;bool available=true; };
struct Snapshot {
    uint64_t context=0;bool ready=false,authority=false,spawn_ready=false,god_ready=false,god=false;
    std::array<Enemy,kEnemies> enemies{};std::vector<Item> items;
    std::string status=u8"Oyun sahnesi bekleniyor.";
    std::string spawn_status=u8"Maç envanteri bekleniyor.",god_status=u8"Maç karakteri bekleniyor.";
    bool ghost_ready=false,ghost=false,infinity_ready=false,infinity=false;
    std::string tools_status=u8"Kendi maç oyuncun bekleniyor.";
    bool explosion_ready=false,fall_ready=false;
    bool player_speed_ready=false;float player_speed=1;
    bool hamarat_ready=false,difficulty_ready=false;
    bool invisible_ready=false,invisible=false;
    std::string invisible_status;
    int difficulty=-1;
    std::string progression_status;
    room_vote::View vote;
};
enum class Kind { PauseEnemy, EnemySettings, ResetEnemy, RemoveEnemy, KillEnemy, CloneEnemy, Spawn, God, Ghost, GhostReturn, InfinityAmmo, StartVote, CancelVote, Difficulty, PlayerSpeed };
// InfinityAmmo is retained for old queued commands; God Mode owns its state.
inline void SyncGodAmmo(Snapshot& s){s.infinity=s.ready&&s.god&&s.god_ready&&s.infinity_ready;}
struct Command { Kind kind;uint64_t context;size_t enemy=0;Enemy settings{};int item=0;bool flag=false;double queued=0;int seconds=10;float clone_distance=4;float factor=1; };
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
    if(value.kind==Kind::PauseEnemy&&value.enemy>=3)return false;
    if(value.kind==Kind::CloneEnemy&&value.enemy>=3)return false;
    if(value.kind==Kind::PlayerSpeed){if(!std::isfinite(value.factor))return false;value.factor=std::clamp(value.factor,1.f,5.f);}
    if(value.kind==Kind::Difficulty&&(value.item<0||value.item>5))return false;
    if(value.kind==Kind::StartVote){if(value.item<=0)return false;value.seconds=std::clamp(value.seconds,1,60);}
    if(value.kind==Kind::EnemySettings||value.kind==Kind::CloneEnemy){
        if(!std::isfinite(value.settings.speed)||!std::isfinite(value.settings.animation)||!std::isfinite(value.settings.size))return false;
        value.settings.speed=std::clamp(value.settings.speed,.1f,5.f);
        value.settings.animation=std::clamp(value.settings.animation,.1f,3.f);
        value.settings.size=std::clamp(value.settings.size,.25f,value.enemy==4?20.f:3.f);
        if(!std::isfinite(value.clone_distance))return false;
        value.clone_distance=std::clamp(value.clone_distance,2.f,6.f);
    }
    value.queued=Now();
    if(value.kind==Kind::PauseEnemy||value.kind==Kind::EnemySettings||value.kind==Kind::Difficulty||value.kind==Kind::PlayerSpeed){
        for(auto& old:commands)if(old.kind==value.kind&&old.context==value.context&&old.enemy==value.enemy){old=value;return true;}
    }
    if(commands.size()>=24)return false;
    commands.push_back(value);return true;
}
inline std::deque<Command> Take(){std::lock_guard<std::mutex> lock(mutex);std::deque<Command> result;result.swap(commands);return result;}
inline void Clear(){std::lock_guard<std::mutex> lock(mutex);commands.clear();}
}
