#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>

int main() {
    std::ifstream file("referances/global-metadata.dat", std::ios::binary);
    if (!file) {
        std::cerr << "Dosya açilamiyor\n";
        return 1;
    }
    
    std::vector<char> buffer(65536);
    std::string text = "";
    std::set<std::string> found;
    
    while (file) {
        file.read(buffer.data(), buffer.size());
        std::streamsize bytesRead = file.gcount();
        
        for (int i = 0; i < bytesRead; i++) {
            unsigned char b = static_cast<unsigned char>(buffer[i]);
            if (b >= 32 && b <= 126) {
                text += static_cast<char>(b);
            } else {
                if (!text.empty() && text.length() % 50 == 0) {
                    text += '\n';
                }
            }
        }
        
        if (text.length() > 0 && text.length() % 500 == 0) {
            std::cout << text.substr(0, 500) << "\n\n";
            text = "";
        }
    }
    
    // Arama fonksiyonlari
    std::cout << "\n=== Aranan Fonksiyonlar ===\n";
    std::vector<std::string> targets = {
        "GodMode", "Invisible", "noclipMode", "UnlimitedAmmo", "GiveFreezeTrap",
        "RemovePlayer", "Object_Instantiate", "SpawnItem", "SetGodMode",
        "ToggleGodMode", "ToggleFreezeTrap", "KillPlayer"
    };
    
    for (const auto& target : targets) {
        if (found.count(target)) {
            std::cout << target << ": BULUNDU\n";
        } else {
            std::cout << target << ": Bulunamadi\n";
        }
    }
    
    return 0;
}
