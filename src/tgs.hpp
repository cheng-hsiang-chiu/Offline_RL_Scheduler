#pragma once

#include <iostream>
#include <atomic>
#include <vector>
#include <memory>
#include <list>
#include <utility>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <sstream>
#include <chrono>
#include <utility>
#include <numeric>
#include <map>

//#include <CL/sycl.hpp>

#include "policy.hpp"

namespace tgs {

class RL_Policy;
class TGS;



struct Task {
  size_t ID;

  // the row dimension of the matrix
  size_t M;

  // the column dimension of the matrix
  size_t N;

  // atomic join_counter
  std::atomic<int> join_counter{0};

  // Task executed on accelerator 
  Accelerator accelerator;

  // woker id the task is executed on
  size_t worker_id;

  // parent id of the task
  std::vector<size_t> parent_id;

  // atomic number of child
  std::atomic<int> num_child{0};

  // pointer to a dynamically allocated matrix 
  int* ptr_matrix = nullptr;

  Task(const size_t id, const size_t m, const size_t n) :
    ID{id}, M{m}, N{n} {
  } 
};

class ThreadPool {

friend class RL_Policy;
friend class TGS;

public:

  bool stop = false;
  
  ThreadPool(const ThreadPool &) = delete;
  
  ThreadPool(ThreadPool &&) = delete;
  
  ThreadPool &operator=(const ThreadPool &) = delete;
  
  ThreadPool &operator=(ThreadPool &&) = delete;
  
  ThreadPool(const size_t, TGS*);

  ~ThreadPool();

  template<typename T>
  void enqueue(T&&);

private:

  std::atomic<size_t> _processed{0};

  size_t _num_threads;

  TGS* _tgs; 

  RL_Policy _rl;

  std::vector<std::mutex> _mtxs;

  std::vector<std::condition_variable> _cvs;

  std::vector<std::thread> _workers;

  std::vector<std::deque<Task*>> _queues;

  template<typename T>
  void _process(size_t, T&&);
};


// task graph scheduler class  
class TGS {

friend class ThreadPool;

friend class RL_Policy;

public:

  TGS(const size_t);

  void dump(std::ostream&) const;

  void schedule();

  ~TGS();

private:
  size_t _V;

  size_t _E;

  std::vector<std::unique_ptr<Task>> _tasks;
  
  std::vector<std::vector<size_t>> _graph;
 
  std::map<size_t, size_t> _worker_assignment;
  
  std::vector<std::chrono::high_resolution_clock::time_point> _timestamp;

  ThreadPool _tpool; 

  void _dump_timestamp() const;
};



// TGS construtor
inline TGS::TGS(const size_t num_threads) : _tpool(num_threads, this) {
  std::cout << num_threads << " concurrent threads are supported.\n";

  std::cin >> _V >> _E;

  _graph.resize(_V);
  _tasks.resize(_V);
  _timestamp.resize(2 * _V);
   
  // parse the meta data of every task
  for (size_t i = 0; i < _V; ++i) {
    size_t id, m, n;
    std::cin >> id >> m >> n;
    _tasks[id] = std::make_unique<Task>(id, m, n);
  }
  
  // parse the edges
  for (size_t i = 0; i < _E; ++i) {
    size_t from, to;
    std::cin >> from >> to;

    _graph[from].push_back(to);

    _tasks[to]->join_counter.fetch_add(1, std::memory_order_relaxed);
    
    _tasks[from]->num_child.fetch_add(1, std::memory_order_relaxed);

    _tasks[to]->parent_id.push_back(from);
  }

  // parse the worker assignment
  for (size_t i = 0; i < _V; ++i) {
    size_t id, worker;

    std::cin >> id >> worker;

    _worker_assignment[id] = worker;
  }
}


inline TGS::~TGS() {
  std::cout << "TGS destructor\n";
  for (auto& w : _tpool._workers) {
    w.join();
  }

  double elapsed = 0;
  for (int i = 1; i < _timestamp.size(); ++i) {
    elapsed += std::chrono::duration_cast<std::chrono::microseconds>(
      _timestamp[i]-_timestamp[i-1]).count();
  }
 
  std::cout << "Elasped time : " << elapsed << " us\n";
}



inline void TGS::schedule() {

  std::vector<Task*> source;

  for(const auto& t: _tasks) {
    if(t->join_counter.load() == 0) {
      source.push_back(t.get());
    }
  }

  for(auto task : source){
    _tpool.enqueue(task);
  }
}


inline void TGS::dump(std::ostream& os) const {
  for (auto& task : _tasks) {
    os << "Task["              << task->ID           << "]\n"
       << "   M : "            << task->M            << '\n'
       << "   N : "            << task->N            << '\n'
       << "   join_counter : " << task->join_counter << '\n';
  }
}

// destructor
inline ThreadPool::~ThreadPool() {
  //for (auto& w : _workers) {
  //  w.join();
  //}
  std::cout << "ThreadPool destructor\n";
}


// constructor
inline ThreadPool::ThreadPool(const size_t num_threads, TGS* t) :
  _queues(num_threads+1), _mtxs(num_threads+1),
  _cvs(num_threads+1), _rl{num_threads}{
  
  //std::cout << "ThreadPool constructor\n"; 
  _num_threads = num_threads;
  _tgs = t;
  
  for (size_t i = 0; i < _num_threads; ++i) {
    
    // definitions of the worker 
    _workers.emplace_back([&, id=i]() {
      Task* task(nullptr);
      while (1) {
        {
          std::unique_lock<std::mutex> lk(_mtxs[id]);
          _cvs[id].wait(lk, [&]() { 
            return !_queues[id].empty() || stop; 
          });
       
          // the scheduler has scheduled all tasks 
          if(stop) {
            return;
          }
          
          // worker i has tasks in its queue
          // and pops a task from the queue
          if(!_queues[id].empty()) {
            task = _queues[id].front();
            _queues[id].pop_front();
          }
        }

        // start to process the task
        _process(id, task);
      }
    });
  }

  // master thread does the scheduling
  _workers.emplace_back([&](){
    Task* task(nullptr);
    size_t idx = 0;
     
    while(1) {
      {
        std::unique_lock<std::mutex> lk(_mtxs[_num_threads]);
        _cvs[_num_threads].wait(lk, [&](){
          return !_queues[_num_threads].empty() || stop;
        }); 

        // the scheduler has scheduled all tasks
        if(stop) {
          return;
        }
       
        // master has tasks in its queue
        // and pops a task from its queue 
        if(!_queues[_num_threads].empty()) {
          task = _queues[_num_threads].front();
          _queues[_num_threads].pop_front();  
        }
      }
    
       _tgs->_timestamp[idx++] = std::chrono::high_resolution_clock::now(); 

      // master begins to call RL for an action regarding the task
      auto policy = _rl.policy_read(task, _tgs);
      
      //printf("Master decides to run task %zu with the policy : worker %ld, accelerator %d\n", 
      //        task->ID, policy.first, policy.second);
      
      _tgs->_timestamp[idx++] = std::chrono::high_resolution_clock::now(); 
      
      task->worker_id = policy.first; 
      
      // record the state and action pair
      //_state_query(*task, policy);
      
      
      // master gets the action recommendation and pushes the task to
      // the corresponding worker's queue
      {
        std::unique_lock<std::mutex> lk(_mtxs[policy.first]);
        task->accelerator = policy.second;
        _queues[policy.first].emplace_back(task);
      }
      
      _cvs[policy.first].notify_one();
    }
  });
}


// enqueue a task in master's queue
template<typename T>
inline void ThreadPool::enqueue(T&& task) {
  // acquire the lock of master thread
  // before push the task into master's queue
  {
    std::unique_lock<std::mutex> lk(_mtxs[_num_threads]);
    _queues[_num_threads].emplace_back(std::forward<T>(task));
  }
  _cvs[_num_threads].notify_one();
}


template<typename T>
inline void ThreadPool::_process(size_t id, T&& task) {

  //std::ostringstream oss;
  //oss << "Worker " << id << " is processing task " << task->ID << std::endl;
  //printf("%s\n", oss.str().c_str());

  // TODO: add SYCL kernel based on the policy
  // right now we use sleep to simulate the loading of a task
  //std::this_thread::sleep_for(std::chrono::microseconds(task->M * task->M * task->N));

  // fetch the result from parents 
  //int parent_sum = 0;
  //for (auto pid : task->parent_id) {
  //  auto& rm = _tgs->_tasks[pid]->result_matrix;
  //  parent_sum += std::accumulate(rm.begin(), rm.end(), 0);
  //}
  // fetch the result from parents 
  int parent_sum = 0;
  for (auto pid : task->parent_id) {
    Task* parent = _tgs->_tasks[pid].get();
    for (int i = 0; i < parent->M * parent->M; ++i) {
      parent_sum += (parent->ptr_matrix)[i];
    }

    if (parent->num_child.fetch_sub(1) == 1) {
      delete [] parent->ptr_matrix;
    }
  }
  
  //std::ostringstream oss;
  //oss << "Worker " << id << " is processing task " << task->ID << " with parent data = " << parent_sum << std::endl;
  //printf("%s\n", oss.str().c_str());

  // use parent_sum to initialize matrix A and B
  std::vector<int> A(task->M * task->N, parent_sum+task->ID);
  std::vector<int> B(task->N * task->M, parent_sum-task->ID);
  task->ptr_matrix = new int[task->M*task->M];
  //std::vector<int> C(task->M * task->M, 0);

  // the following does matrix multiplication
  for (size_t i = 0; i < task->M; ++i) {
    for (size_t j = 0; j < task->M; ++j) {
      task->ptr_matrix[i*task->M+j] = 0;
      for (size_t k = 0; k < task->N; ++k) {
        task->ptr_matrix[i*task->M+j] += A[i*task->N+k] * B[k*task->M+j];  
      }
    }
  }

  // i am a node with no child
  // delete the pointer directly
  if (task->num_child == 0) {
    delete [] task->ptr_matrix;
  }
  
  // decrement the dependencies
  for (auto& tid : _tgs->_graph[task->ID]) {
    if(_tgs->_tasks[tid]->join_counter.fetch_sub(1)==1){
      enqueue(_tgs->_tasks[tid].get());
    }  
  }

  // finished processing all tasks
  // stop the scheduler
  if (_processed.fetch_add(1) + 1 == _tgs->_V) {
    stop = true;
    for(auto& cv : _cvs) {
      cv.notify_one();
    }

    // dump the timestamp 
    // used for plotting histogram only
    //_tgs->_dump_timestamp();

    // dump state_action_tuples
    //dump_state_action_tuples();
  }
}









} // end of namespace tgs
