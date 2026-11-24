#include "../src/photo_transport_bridge.h"
#include "../src/photo_transfer_security.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace photo_transport_bridge;
    static_assert(photo_transfer_security::kMaxImageBytes>=8*1024*1024,"256 KB prototype limit still active");
    ChooseRemoteFile("one.jpg");auto c=Take();assert(c.choose_remote_file=="one.jpg"&&!c.fetch);
    ChooseRemoteFile("two.png");c=Take();assert(c.choose_remote_file=="two.png"&&!c.fetch);
    SelectAll();c=Take();assert(c.select_all);
    ClearSelection();c=Take();assert(c.clear_selection);
    Fetch();c=Take();assert(c.fetch);
    StopSharing();c=Take();assert(c.stop_sharing);
    std::cout << "GMM batch commands and session revocation: PASS\n";
}
