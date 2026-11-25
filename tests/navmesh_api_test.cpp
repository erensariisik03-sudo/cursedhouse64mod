#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <set>
#include <string>
static std::string Read(const char* path){
    std::ifstream in(path);assert(in.good());return {(std::istreambuf_iterator<char>(in)),{}};
}
int main(){
    // Captured verbatim from the supplied player's dump.cs. Unity's public
    // documentation lists many APIs that were stripped from this specific APK.
    const auto metadata=Read("tests/fixtures/navmesh_agent_dump.txt");
    std::set<std::string> methods;
    const std::regex declaration(R"(\b(\w+)\([^\n]*\) \{ \})");
    for(auto i=std::sregex_iterator(metadata.begin(),metadata.end(),declaration);i!=std::sregex_iterator();++i)methods.insert((*i)[1]);
    assert(methods.count("SetDestination")&&methods.count("get_speed")&&methods.count("set_speed"));
    assert(!methods.count("Warp")&&!methods.count("get_isOnNavMesh")&&!methods.count("set_isStopped"));
    const auto source=Read("src/gameplay.cpp");
    const std::regex lookup("ResolveMethod\\(\"UnityEngine.AI\",\"NavMeshAgent\",\"([^\"]+)\"");
    size_t checks=0;
    for(auto i=std::sregex_iterator(source.begin(),source.end(),lookup);i!=std::sregex_iterator();++i){
        if(!methods.count((*i)[1])){std::cerr<<"NavMesh method missing from actual player: "<<(*i)[1]<<'\n';return 1;}++checks;
    }
    assert(checks>=3);
    std::cout<<"Actual stripped NavMesh metadata: all gameplay navigation lookups exist; absent Warp/isOnNavMesh/isStopped cannot gate pause or clone startup: PASS\n";
}
