#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdio>
#include <sys/stat.h>
namespace myinstants {
struct Sound {std::string title,path;};
inline std::string Encode(const std::string& text){const char* hex="0123456789ABCDEF";std::string out;for(unsigned char c:text){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')out+=char(c);else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
inline bool ValidPath(const std::string& p){constexpr const char* base="/media/sounds/";if(p.compare(0,14,base)!=0||p.size()<19||p.size()>500||p.substr(p.size()-4)!=".mp3"||p.find("..")!=std::string::npos)return false;for(size_t i=14;i<p.size();++i){unsigned char c=p[i];if(!std::isalnum(c)&&c!='_'&&c!='-'&&c!='.'&&c!='%')return false;if(c=='%'){if(i+2>=p.size()||!std::isxdigit(static_cast<unsigned char>(p[i+1]))||!std::isxdigit(static_cast<unsigned char>(p[i+2])))return false;const auto h=[](unsigned char x){return x<='9'?x-'0':std::tolower(x)-'a'+10;};int value=h(p[i+1])*16+h(p[i+2]);if(value!=32&&value<128)return false;i+=2;}}return true;}
inline std::string HtmlText(std::string s){std::string out;bool tag=false;for(char c:s){if(c=='<')tag=true;else if(c=='>')tag=false;else if(!tag)out+=c;}for(const auto& e:std::vector<std::pair<std::string,std::string>>{{"&amp;","&"},{"&quot;","\""},{"&#39;","'"},{"&#x27;","'"},{"&lt;","<"},{"&gt;",">"}}){size_t pos=0;while((pos=out.find(e.first,pos))!=std::string::npos){out.replace(pos,e.first.size(),e.second);pos+=e.second.size();}}const auto a=out.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};return out.substr(a,out.find_last_not_of(" \t\r\n")-a+1);}
inline std::vector<Sound> Parse(const std::string& html){std::vector<Sound> out;size_t at=0;while(out.size()<24&&(at=html.find("play(",at))!=std::string::npos){at+=5;while(at<html.size()&&std::isspace(static_cast<unsigned char>(html[at])))++at;if(at>=html.size()||(html[at]!='\''&&html[at]!='\"'))continue;char quote=html[at++];auto end=html.find(quote,at);if(end==std::string::npos)break;std::string path=html.substr(at,end-at);at=end+1;const std::string origin="https://www.myinstants.com";if(path.rfind(origin,0)==0)path.erase(0,origin.size());if(!ValidPath(path))continue;if(std::any_of(out.begin(),out.end(),[&](const Sound& s){return s.path==path;}))continue;std::string title;auto next=html.find("play(",at),link=html.find("instant-link",at);if(link!=std::string::npos&&(next==std::string::npos||link<next)){auto open=html.find('>',link),close=open==std::string::npos?std::string::npos:html.find("</a",open);if(close!=std::string::npos&&close-open<1000&&(next==std::string::npos||close<next))title=HtmlText(html.substr(open+1,close-open-1));}if(title.empty())title=path.substr(14,path.size()-18);out.push_back({std::move(title),std::move(path)});}return out;}
inline constexpr size_t kMaxFavorites=100;
inline bool IsFavorite(const std::vector<Sound>& list,const std::string& path){return std::any_of(list.begin(),list.end(),[&](const Sound& s){return s.path==path;});}
inline bool ToggleFavorite(std::vector<Sound>& list,const Sound& sound){
 auto it=std::find_if(list.begin(),list.end(),[&](const Sound& s){return s.path==sound.path;});
 if(it!=list.end()){list.erase(it);return true;}
 if(list.size()>=kMaxFavorites||!ValidPath(sound.path)||sound.title.empty()||sound.title.size()>1000||sound.title.find_first_of("\r\n")!=std::string::npos)return false;
 list.push_back(sound);return true;
}
inline std::string SerializeFavorites(const std::vector<Sound>& list){std::ostringstream out;for(const auto& s:list)out<<std::quoted(s.path)<<' '<<std::quoted(s.title)<<'\n';return out.str();}
inline std::vector<Sound> ParseFavorites(std::istream& in){std::vector<Sound> list;std::string line;while(list.size()<kMaxFavorites&&std::getline(in,line)){if(line.size()>4096)continue;std::istringstream row(line);Sound s;if(row>>std::quoted(s.path)>>std::quoted(s.title)&&!IsFavorite(list,s.path))ToggleFavorite(list,s);}return list;}
inline std::string FavoritesPath(){
 std::ifstream cmd("/proc/self/cmdline",std::ios::binary);std::string pkg;std::getline(cmd,pkg,'\0');
 if(pkg.empty()||pkg.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._")!=std::string::npos||pkg.find("..")!=std::string::npos)return {};
 for(const char* base:{"/data/user/0/","/data/data/"}){auto dir=std::string(base)+pkg+"/files";struct stat st{};if(stat(dir.c_str(),&st)==0&&S_ISDIR(st.st_mode))return dir+"/gmm_myinstants_favorites.cfg";}
 return {};
}
inline bool SaveFavorites(const std::string& path,const std::vector<Sound>& list){
 if(path.empty())return false;
 const auto temp=path+".tmp";std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out)return false;out<<SerializeFavorites(list);out.close();if(!out||chmod(temp.c_str(),0600)!=0||std::rename(temp.c_str(),path.c_str())!=0){std::remove(temp.c_str());return false;}return true;
}
}
