#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <exception>
#include <functional>


/**
 * TCPSender responsiblity to:
 *      1. 记录receiver的window大小和acknos
 *      2. 尽可能地填补receiver的window,通过从ByteStream读取,创建新的TCP segment,之后发送它们.
 *          发送者会一直发送segments,直到window满了或者是outbound已经没有数据传输了
 *      3. 记录那些segment已经发送了,但是没被receiver
 *      4. 如果已经发送的segment,在发送后经过过足够的时间而且还没有收到确认,那就重新发送这些未确认的segment
 */


class TCPSender
{
private:
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  bool syn_ = false;
  bool fin_ = false;
  uint64_t next_seqno_{0};   // 记录sender已经发送多少个字节（包含第一次握手）
  uint64_t recev_seqno_{0};  // 记录sender已经接受了多少个字节）
  uint16_t window_size_{1};  // receiver的滑动窗口的大小
  std::deque<TCPSenderMessage> outstanding_messages_{}; // sender已经发送，但是还没有收到receiver回复的ack
  std::deque<TCPSenderMessage> messages_{}; // Sender 准备发送的sender messages
  uint64_t ncr_{0}; // the number of consecutive retransmissions

  // 定时器
  size_t timestamp_{0}; // 只从上次调用已经过了多久了
  bool active_{false};
  uint64_t cur_RTO_{initial_RTO_ms_};

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};

