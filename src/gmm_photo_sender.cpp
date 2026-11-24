#include "photo_transfer_security.h"
#include "photo_browse_protocol.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// GMM photo sender: only owner-side listing and safe, explicitly approved
// Pictures image loading. No Photon event, auto-sharing, or kick code here.
namespace gmm_photo_sender {
namespace {
const std::string kShareDir="/sdcard/Pictures";
using photo_transfer_security::Bytes;
using photo_transfer_security::IsImage;
using photo_transfer_security::kMaxImageBytes;
}
std::vector<std::string> ListImages() {
    std::vector<std::string> files;
    DIR* dir=opendir(kShareDir.c_str());if(!dir)return files;
    for(dirent* entry;(entry=readdir(dir))!=nullptr && files.size()<100;){
        const std::string name=entry->d_name;
        if(!photo_browse_protocol::NameOk(name))continue;
        struct stat st{};const std::string full=kShareDir+"/"+name;
        if(lstat(full.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=8&&
           st.st_size<=static_cast<off_t>(kMaxImageBytes))files.push_back(name);
    }
    closedir(dir);std::sort(files.begin(),files.end());return files;
}
bool LoadImage(const std::string& filename,Bytes& bytes) {
    bytes.clear();
    if(!photo_browse_protocol::NameOk(filename))return false;
    const std::string path=kShareDir+"/"+filename;
    const int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW);
    if(fd<0)return false;
    struct stat st{};
    const bool allowed=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=8&&
       st.st_size<=static_cast<off_t>(kMaxImageBytes);
    if(!allowed){close(fd);return false;}
    bytes.resize(static_cast<size_t>(st.st_size));size_t received=0;bool ok=true;
    while(received<bytes.size()){
        const ssize_t n=read(fd,bytes.data()+received,bytes.size()-received);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){ok=false;break;}
        received+=size_t(n);
    }
    if(close(fd)!=0)ok=false;
    if(!ok||!IsImage(bytes)){bytes.clear();return false;}
    return true;
}
} // namespace gmm_photo_sender
