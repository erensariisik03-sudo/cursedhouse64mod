#pragma once
// Gemini isteği + model yedekleme. Ağ işi ayrı thread'de yapılır; Unity thread'i
// yalnızca Submit/Poll çağırır (bloklanmaz). HTTP işlevi enjekte edilir (host testi).
#include "chat_bot_core.h"
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace chat_bot_engine {
using HttpFn = bool (*)(const std::string& url, const std::string& key, const std::string& body, int& code, std::string& response);
inline constexpr std::array<const char*, 5> kModels = {
    "gemini-3.5-flash-lite", "gemini-3.1-flash-lite-preview", "gemini-3-flash-preview",
    "gemini-flash-lite-latest", "gemini-3.1-flash-lite"};
inline constexpr int kModelTimeoutMs=10000;
struct Result {
    bool ok=false, empty=false, all_failed=false;
    int code=0; long long retry_ms=0;
    std::string text,model;
};
struct Pool {
    std::atomic<size_t> last{0};
    std::atomic<int> last_error{0};
    std::array<std::atomic<bool>,kModels.size()> in_flight{}; // avoid overlapping unresolved HTTP requests only
};
inline std::mutex pools_mutex;
inline std::string pool_key;
inline std::shared_ptr<Pool> current_pool=std::make_shared<Pool>();
inline std::shared_ptr<Pool> PoolFor(const std::string& key){
    std::lock_guard<std::mutex> l(pools_mutex);
    if(key!=pool_key){pool_key=key;current_pool=std::make_shared<Pool>();}
    return current_pool;
}
inline bool ExplicitBlock(const std::string& response){
    return response.find("\"blockReason\"")!=std::string::npos||
      (response.find("\"finishReason\"")!=std::string::npos&&
       (response.find("\"SAFETY\"")!=std::string::npos||response.find("\"RECITATION\"")!=std::string::npos||response.find("\"PROHIBITED_CONTENT\"")!=std::string::npos||response.find("\"BLOCKLIST\"")!=std::string::npos));
}
inline Result Generate(HttpFn http,const std::shared_ptr<Pool>& pool,const std::string& system,
                       const std::vector<chat_bot_core::Turn>& history,const std::string& key,unsigned tokens=900,
                       const std::shared_ptr<std::atomic<bool>>& canceled={},int timeout_ms=kModelTimeoutMs){
    Result r;r.code=pool->last_error.load();
    const size_t start=pool->last.load()%kModels.size();
    for(size_t offset=0;offset<kModels.size();++offset){
        const size_t i=(start+offset)%kModels.size();
        if(canceled&&canceled->load())break;
        const std::string model=kModels[i];
        bool idle=false;if(!pool->in_flight[i].compare_exchange_strong(idle,true))continue;
        struct Attempt{bool sent=false;int code=0;std::string response;};
        auto promise=std::make_shared<std::promise<Attempt>>();auto future=promise->get_future();
        const auto body=chat_bot_core::BuildRequest(system,history,model,tokens);
        try{
            std::thread([http,pool,i,model,key,body,promise]{
                Attempt a;try{a.sent=http("https://generativelanguage.googleapis.com/v1beta/models/"+model+":generateContent",key,body,a.code,a.response);}catch(...){}
                pool->in_flight[i]=false;promise->set_value(std::move(a));
            }).detach();
        }catch(...){pool->in_flight[i]=false;promise->set_value(Attempt{});}
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);
        while(future.wait_until(std::min(deadline,std::chrono::steady_clock::now()+std::chrono::milliseconds(50)))!=std::future_status::ready){
            if((canceled&&canceled->load())||std::chrono::steady_clock::now()>=deadline)break;
        }
        if(canceled&&canceled->load())break;
        const bool ready=future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;
        auto attempt=ready?future.get():Attempt{};
        r.code=ready?(attempt.sent?attempt.code:0):408;
        if(attempt.sent&&attempt.code==200){
            if(ExplicitBlock(attempt.response)){pool->last=i;r.empty=true;return r;}
            auto text=chat_bot_core::ExtractText(attempt.response);
            if(text&&!chat_bot_core::Trim(*text).empty()){r.ok=true;r.text=*text;r.model=model;pool->last=i;pool->last_error=0;return r;}
        }
        pool->last_error=r.code;
        pool->last=(i+1)%kModels.size();
        // Invalid credentials are shared by every model; do not retry them.
        if(r.code==401)break;
    }
    r.all_failed=true;
    return r; // one full pass per message; next message may retry immediately
}
class Engine {
    struct Shared {std::mutex mutex;bool done=false;Result result;};
    HttpFn http_;std::shared_ptr<Shared> job_;std::shared_ptr<Pool> pool_=std::make_shared<Pool>();
public:
    explicit Engine(HttpFn http):http_(http){}
    bool Busy()const{return bool(job_);}
    std::string Model()const{return kModels[pool_->last.load()%kModels.size()];}
    void Reset(){pool_->last=0;pool_->last_error=0;}
    bool Submit(const std::string& system,const std::vector<chat_bot_core::Turn>& history,const std::string& key){
        if(job_)return false;
        auto shared=std::make_shared<Shared>();job_=shared;pool_=PoolFor(key);auto pool=pool_;auto http=http_;
        std::thread([=]{auto r=Generate(http,pool,system,history,key);std::lock_guard<std::mutex> l(shared->mutex);shared->result=std::move(r);shared->done=true;}).detach();return true;
    }
    bool Poll(Result& out){if(!job_)return false;{std::lock_guard<std::mutex> l(job_->mutex);if(!job_->done)return false;out=std::move(job_->result);}job_.reset();return true;}
};
}
