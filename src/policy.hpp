#pragma once

#include <utility>
#include <random>
#include <fstream>
#include <string>
#include "tgs.hpp"


namespace tgs {

enum Accelerator {
  GPU = 0,
  CPU
};



class RL_Policy {

public:
  //RL_Policy() = default;

  RL_Policy(const size_t n): _num_threads{n} { /*std::cout << "RL Policy constructor\n";*/ }

  template<typename T, typename C>
  std::pair<size_t, Accelerator> policy_read(T&&, C*) const;

  ~RL_Policy(){};

private:
  size_t _num_threads;
};


template<typename T, typename C>
inline std::pair<size_t, Accelerator>
RL_Policy::policy_read(T&& task, C* tgs) const {
  //std::cout << task->ID << ' '
  //          << task->M  << ' '
  //          << task->N  << ' '
  //          << task->join_counter.load() << '\n';
  

  //for (auto& ws : tgs->_worker_assignment) {
  //  std::cout << ws.first << ", " << ws.second << '\n';
  //}

  size_t thread_id = tgs->_worker_assignment[task->ID];
  Accelerator accelerator{Accelerator::CPU};

  return std::make_pair(thread_id, accelerator); 
}




}
