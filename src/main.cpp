#include <iostream>
#include "tgs.hpp"
#include <chrono>



int main() {

  tgs::TGS scheduler(std::thread::hardware_concurrency());

  // use 8 worker threads  
  //tgs::TGS scheduler(8);

  //scheduler.dump(std::cout);

  scheduler.schedule();

  return 0;
}
