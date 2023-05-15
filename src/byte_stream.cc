#include <stdexcept>
#include <iostream>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), buffer_(queue<char>{}),
                                              is_close_(false), error_(false), pushed_count_{0}, popped_count_{0}{
}

void Writer::push( string data )
{
  auto avail = available_capacity();
  if (avail == 0 || data.empty()){
    return ;
  }
  uint idx = 0;
  for(size_t i=avail;i>0;i--){
    buffer_.push(data[idx++]);
    pushed_count_++;
    if (idx == data.size()){
      break ;
    }
  }
}

void Writer::close()
{
  // Your code here.
  is_close_ = true;
}

void Writer::set_error()
{
  // Your code here.
  error_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return is_close_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return pushed_count_;
}

string_view Reader::peek() const
{
  // Your code here.
  if (has_error() || is_finished()){
    return "";
  }
  const std::string_view ans(&buffer_.front(), 1);
  return ans;
}

bool Reader::is_finished() const
{
  // Your code here.
  return is_close_ && pushed_count_ == popped_count_;
}

bool Reader::has_error() const
{
  // Your code here.
  return error_;
}

void Reader::pop( uint64_t len )
{
  // buffer 移除需要注意，需要移除的大小大于buffer当前所储存的字节
  u_int64_t const real_len = len > bytes_buffered()?bytes_buffered():len;
  for(uint64_t i=0;i<real_len;i++){
    buffer_.pop();
    popped_count_++;
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return popped_count_;
}
