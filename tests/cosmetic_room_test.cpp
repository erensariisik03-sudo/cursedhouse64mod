#include "cosmetic_room.h"
#include <cassert>
#include <iostream>
using cosmetic_room::Plan;
int main(){
 Plan p;p.Context("account-a",false,1,0);p.Select(1,"skin-a");assert(!p.Due(1,0,false));
 p.Context("account-a",true,1,0);assert(p.Due(1,0,false));p.Attempt(1,0,true);
 assert(!p.Due(1,.5,false)&&!p.Due(1,2,true));
 assert(!p.Due(1,2,std::nullopt)); // No repeated sends when readback is unavailable.
 assert(p.Due(1,2,false));p.Attempt(1,2,true);assert(p.Due(1,4,false));p.Attempt(1,4,true);
 assert(!p.Due(1,9,false)); // A rejected/mismatched value does not flood the room.
 p.Context("account-a",true,1,42);assert(p.Due(1,10,true));p.Attempt(1,10,true);
 assert(!p.Due(1,12,true)); // Republish once on room entry, not every frame.
 p.Context("account-a",false,1,0);assert(!p.Due(1,13,false));
 p.Context("account-a",true,2,99);assert(p.Due(1,14,false));p.Attempt(1,14,true);
 p.Select(0,"");assert(p.Wanted(0)&&p.Wanted(0)->empty()&&p.Due(0,15,false)); // Remove a hat.
 p.Select(1,"");assert(p.Wanted(1)=="skin-a"); // Invalid empty skin is ignored.
 p.Select(1,"skin-b");assert(p.Due(1,15,false));p.Attempt(1,15,false);
 assert(!p.Due(1,15.5,false)&&p.Due(1,17,false));
 // Closing/reopening a wardrobe does not overwrite the remembered explicit click.
 p.Context("account-a",true,2,99);assert(p.Wanted(1)=="skin-b");
 p.Context("account-b",true,2,99);assert(!p.Wanted(0)&&!p.Wanted(1));
 p.Context("",true,2,99);p.Select(1,"skin-c");assert(!p.Wanted(1));
 std::cout<<"Cosmetic room plan: explicit selections, offline/reconnect/room transitions, empty-hat removal, local readback, bounded retries and account isolation: PASS\n";
}
