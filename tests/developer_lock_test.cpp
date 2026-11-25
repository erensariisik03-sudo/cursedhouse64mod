#include "developer_lock.h"
#include <cassert>
#include <iostream>
int main(){
  developer_lock::Session s;
  assert(!s.Unlocked());
  assert(!s.Verify("YOBAZALI03"));
  assert(s.AttemptsRemaining()==4);
  assert(s.Verify(u8"YOBAZALİ03"));
  assert(s.Unlocked()&&s.AttemptsRemaining()==5);
  s.Lock();assert(!s.Unlocked());
  developer_lock::Session fail;
  for(int i=0;i<5;++i)assert(!fail.Verify("wrong"));
  assert(!fail.Verify(u8"YOBAZALİ03"));
  photo_transfer_security::Gate gate;
  assert(!gate.SetReceiving(true));
  gate.AuthorizeVerifiedSession();assert(gate.Unlocked());
  assert(gate.SetReceiving(true));gate.Lock();assert(!gate.Receiving());
  std::cout<<"Developer fixed password + receiver gate: PASS\n";
}
