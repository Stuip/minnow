#include "tcp_receiver.hh"
#include <iostream>

using namespace std;

/**
 * 接受到来自peer的message,使用reassembler将message写入到
 */
void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  if (message.SYN) {
    // 记录SYN
    zero_point = message.seqno;
    message.seqno = message.seqno + 1;
    isSYN_ = true;
  }
  if (!zero_point.has_value()) {
    //如果没有SYN,则直接返回
    return;
  }
  if (!message.payload.empty()) {
    auto first_index_ = message.seqno.unwrap(zero_point.value(), inbound_stream.bytes_pushed()) - 1;
    reassembler.insert(first_index_, message.payload.release(), message.FIN, inbound_stream.writer());
  }
  if (message.FIN) {
    isFIN_ = true;
    if (reassembler.bytes_pending() == 0) {
      inbound_stream.close();
    }
  }
}

/**
 * Receiver 需要响应的窗口大小和ackno和window size
 *  ackno 指的是Receiver接下来需要的sequence number
 */
TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  (void)inbound_stream;
  const uint16_t windowSize_ = inbound_stream.available_capacity() >= UINT16_MAX + 1?UINT16_MAX:inbound_stream.available_capacity();
  if (!zero_point.has_value()) {
    return {std::optional<Wrap32>{}, windowSize_};
  }
  auto offset = 0;
  if (isSYN_) offset += 1;
  if (isFIN_ && inbound_stream.is_closed()) offset += 1;
  return {zero_point.value() + inbound_stream.bytes_pushed() + offset, windowSize_};
}
