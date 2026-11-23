#pragma once
#include <cstddef>
namespace gameplay_catalog {
// PickUpObject values from the supplied dump; labels never become resource keys.
inline const char* Name(int id){
    static constexpr const char* names[]={
        "None","Cutting Pliers","Vase","Hammer","Vase 2","Safe Key","House Key","Padlock Key","Padlock Code","Crossbow","Weapon Key","Screwdriver","Wooden Plank","Door Battery",
        "Painting Piece 1","Painting Piece 2","Painting Piece 3","Painting Piece 4","Playhouse Key","Melon","Teddy Bear","Cogwheel 1","Cogwheel 2","Message Note","Winch Handle","Shotgun",
        "Shotgun Part 1","Shotgun Part 2","Shotgun Part 3","Car Key","Car Engine","Car Battery","Gasoline Can","Wrench","Spark Plug","Meat","Special Key","Book","Pepper Spray","Remote Control","Birdseed","Freeze Trap","Spider Key","Chain Cutter","Rusty Padlock Key","Wheel Crank","Wooden Stick","Tranquilizer Dart","Shotgun Ammo"
    };
    static_assert(sizeof(names)/sizeof(*names)==49);
    return id>=0&&static_cast<size_t>(id)<sizeof(names)/sizeof(*names)?names[id]:"Unknown Item";
}
}
