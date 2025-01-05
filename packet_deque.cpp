#include "packet_deque.h"
#include "decodec.h"
extern "C" {
#include "log.h"
#include <libavformat/avformat.h>
}

PacketQueue::PacketQueue(int max_size,std::string queuename) {
  queue_size_ = 0;
  max_size_ = max_size;
  deque_name_ = queuename;
}

PacketQueue::~PacketQueue() { log_debug("PacketQueue Release!"); }


void PacketQueue::Push(void *pkt) {
     //log_debug("push befor mutex queue_size = %d",queue_size_);
   {
     std::unique_lock<std::mutex> lock(mutex_); 
     // log_debug("push befor wait queue_size = %d",queue_size_);
    // 使用单一条件变量，等待直到队列未满
     condition_.wait(lock, [this]() { return (queue_size_ < max_size_); });
    // 执行 Push 操作

     packets_deque_.emplace_back(pkt);
    queue_size_++;
   // log_debug(" %s push queue_size = %d ",deque_name_.c_str(), queue_size_.load());
    }

    // 唤醒等待 Pop 的线程
    condition_.notify_one();  // 因为同一个条件变量，要通知所有等待
}

void *PacketQueue::Pop() {
     //log_debug("pop befor mutex queue_size = %d",queue_size_);
    void *pkt = nullptr;
     {
std::unique_lock<std::mutex> lock(mutex_);
 //log_debug("pop befor wait queue_size = %d",queue_size_);
    // 使用单一条件变量，等待直到队列非空
    condition_.wait(lock, [this]() { return !packets_deque_.empty(); });

    // 执行 Pop 操作
    pkt = packets_deque_.front();
    packets_deque_.pop_front();
    queue_size_--;
    //log_debug("%s pop queue_size = %d", deque_name_.c_str(), queue_size_.load());
     }
    // 唤醒等待 Push 的线程
    condition_.notify_one();  // 因为同一个条件变量，要通知所有等待
    return pkt;
}

void *PacketQueue::Peek() {
     //log_debug("pop befor mutex queue_size = %d",queue_size_);
    void *pkt = nullptr;
     {
std::unique_lock<std::mutex> lock(mutex_);
 //log_debug("pop befor wait queue_size = %d",queue_size_);
    // 使用单一条件变量，等待直到队列非空
    condition_.wait(lock, [this]() { return !packets_deque_.empty(); });

    // 执行 Pop 操作
    pkt = packets_deque_.front();
    //log_debug("%s pop queue_size = %d", deque_name_.c_str(), queue_size_.load());
     }


    // 唤醒等待 Push 的线程
    condition_.notify_one();  // 因为同一个条件变量，要通知所有等待
    return pkt;
}

void *PacketQueue::PeekNext() {
     //log_debug("pop befor mutex queue_size = %d",queue_size_);
    void *pkt = nullptr;
     {
std::unique_lock<std::mutex> lock(mutex_);
 //log_debug("pop befor wait queue_size = %d",queue_size_);
    // 使用单一条件变量，等待直到队列非空
    condition_.wait(lock, [this]() { return !packets_deque_.empty(); });

    // 执行 Pop 操作
    if(packets_deque_.size() > 1) {
      pkt = packets_deque_[1];
    } else {
      log_error("Deque has less than 2 elements.");
      return nullptr;
    }
     }


    // 唤醒等待 Push 的线程
    condition_.notify_one();  // 因为同一个条件变量，要通知所有等待
    return pkt;
}




// void PacketQueue::PushNullpacket() {
//   std::lock_guard<std::mutex> lock(mutex_);
//   void *pkt = av_packet_alloc();
//   av_init_packet(pkt);
//   packets_deque_.push(pkt);
//   queue_size_++;
//   condition_.notify_all();
// }
int PacketQueue::Size() {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_size_;
}
bool PacketQueue::Empty() {
  std::lock_guard<std::mutex> lock(mutex_);
  return packets_deque_.empty();
}

void PacketQueue::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  if(deque_name_ == "vidoeframe"||deque_name_=="audioframe") {
      while (!packets_deque_.empty()) {
    Frame*frame = (static_cast<Frame*>(packets_deque_.front()));
    packets_deque_.pop_front();
    av_frame_free(&frame->frame);
    delete frame;
    queue_size_--;
  }
  } else {
      while (!packets_deque_.empty()) {
    AVPacket*pkt = (static_cast<AVPacket*>(packets_deque_.front()));
    packets_deque_.pop_front();
    av_packet_free(&pkt);
    queue_size_--;
  }
  }

  condition_.notify_all(); // 通知所有等待的线程
}


bool PacketQueue::Full()  {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_size_ == max_size_;
}
