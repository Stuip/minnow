#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <iostream>

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ),
  initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return next_seqno_-recev_seqno_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return ncr_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
    if (messages_.empty()) {
        return {};
    }
    TCPSenderMessage message;
    message = messages_.front();
    messages_.pop_front();
    // 放入到已经发送但是没有ACK的数据报集合中
    outstanding_messages_.push_back(message);
    if (!active_) {
        // 启动定时器
        active_ = true;
    }
    return message;
}

void TCPSender::push( Reader& outbound_stream )
{
    // 如果fin已经发送的话，说明byteStream中并没有多余的字节
    if (fin_) {
        return;
    }
    const uint64_t cur_window_size = window_size_==0?1:window_size_;
    while (cur_window_size > sequence_numbers_in_flight()) {
        // 首先要保证窗口有还有空余位置去发送
        TCPSenderMessage message;
        if (!syn_) {
            syn_ = true;
            message.SYN = true;
        }

        message.seqno = isn_ + next_seqno_;

        auto len = min(min(TCPConfig::MAX_PAYLOAD_SIZE,
                           static_cast<size_t >(outbound_stream.bytes_buffered())), cur_window_size-sequence_numbers_in_flight());


        read(outbound_stream, len, message.payload);

        if (!fin_ && outbound_stream.is_finished() &&
            len + message.SYN + sequence_numbers_in_flight() < cur_window_size) {
            fin_ = true;
            message.FIN = true;
        }

        if (message.sequence_length() == 0) {
            break;
        }

        messages_.push_back(message);
        next_seqno_ += message.sequence_length();
        if (message.FIN || outbound_stream.bytes_buffered() == 0) {
            break;
        }
    }
}

/***
 * 产生一个长度为0的数据报，其中序列号设置正确。
 * 主要的目的是为了当receiver发送一个TCPReceiverMessage，Sender产生一个正确seqno的TCPSender
 */
TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
    TCPSenderMessage message{};
    message.seqno = isn_ + next_seqno_;
    return message;
}

/**
 * 这个函数主要是浏览sender已经发送的但是没有acknowledge的segment的集合，
 * 将那些已经被sender acknowledge的从集合中删除
 */
void TCPSender::receive( const TCPReceiverMessage& msg )
{
    if (msg.ackno.has_value()) {
        auto rv_seqno = msg.ackno->unwrap(isn_, next_seqno_);
        if (rv_seqno > next_seqno_ || rv_seqno < recev_seqno_) {
            // 如果ACK的receive_seqno大于next_seqno的话，则直接返回
            // 因为receiver接受到还没有发送的数据。
            return;
        }
        recev_seqno_ = msg.ackno->unwrap(isn_, next_seqno_);
    }

    window_size_ = msg.window_size;

    while (!outstanding_messages_.empty()) {
        auto segment = outstanding_messages_.front();
        auto last_seqno = (segment.seqno + segment.sequence_length()).unwrap(isn_, next_seqno_);
        if (last_seqno <= recev_seqno_) {
            outstanding_messages_.pop_front();
            // 如果sender收到的有效的ackno，则需要你重置定时器
            if (window_size_ != 0){
                timestamp_ = 0;
                cur_RTO_ = initial_RTO_ms_;
                ncr_ = 0;
            }
        } else {
            break;
        }
    }

    // 如果已发送但未认可的segment没有了，就关闭定时器
    active_ = !outstanding_messages_.empty();

}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
    if (active_) {
        timestamp_ += ms_since_last_tick;
        if (timestamp_ >= cur_RTO_ && !outstanding_messages_.empty()) {
            // 定时器已经失效，所以需要重新传递最久的数据报
            messages_.push_back(outstanding_messages_.front());
            if (window_size_ > 0) {
                ncr_ += 1;
                cur_RTO_ = pow(2, ncr_) * initial_RTO_ms_;
            }
            timestamp_ = 0;
        }
    }
}

