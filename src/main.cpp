#include <iostream>
#include "tgs.hpp"
#include <chrono>



int main() {

  //tgs::TGS scheduler(std::thread::hardware_concurrency());

  // use 8 worker threads  
  tgs::TGS scheduler(8);

  //scheduler.dump(std::cout);

  auto beg = std::chrono::high_resolution_clock::now(); 
  scheduler.schedule();
  auto end = std::chrono::high_resolution_clock::now();

  std::cout << "Elapsed time : "
            << std::chrono::duration_cast<std::chrono::microseconds>(end - beg).count()
            << " us\n"; 


  return 0;
}
