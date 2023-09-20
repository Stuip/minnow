#include <utility>
#include <iostream>
#include "reassembler.hh"

using namespace std;



/**
 * Note:
 *      1. 如果推送的字节是字节流中随后字节，则立即将该字节推送到字节中
 *      2. 如果字节的容量能够该推送的字节中，但是这些字节不能立刻推送到字节流中，
 *          因为之前的字节并不知道，所以这些字节会储存到Reassembler中
 *      3. 如果字节超出字节流所能容纳的大小，则丢弃这些字节。
 *          Reassembler不会储存那些不能立即推送的字节和任何之前字节已知的情况下无法推送的字节
 */
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  if (data.empty()) {
    if (is_last_substring) {
      output.close();
      return;
    }
  }

  auto remain = output.available_capacity();
  auto firstUnpoppedIndex = output.bytes_pushed();
  if (remain == 0 || first_index >= firstUnpoppedIndex + remain || firstUnpoppedIndex >= first_index + data.length()) {
    // 集合器剩余容量为空 或者是 提前到来的字节流当前容量储存不下 或是 已经接受的字节, 则字节流直接drop
    // Note 第二条件可能有问题 >=
    return;
  }

  if (first_index > firstUnpoppedIndex) {
    // 有些字节流提前到来, 则在Reassembler中储存
    insertToBuffer(first_index, data, is_last_substring);
    return;
  }

  // overlapping insert
  // 字符串都的一部分已经插入了,另一部分没有插入
  if (first_index < firstUnpoppedIndex && firstUnpoppedIndex < first_index + data.length()) {
    auto bIdx = firstUnpoppedIndex - first_index;
    auto subStrs = data.substr(bIdx);
    output.push(subStrs);
  }

  if (firstUnpoppedIndex == first_index) {
    output.push(data);
  }
  checkBuffer(output);
  if (is_last_substring) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  auto size_ = 0;
  for (const auto& item:buffer_) {
    size_ += item.second.first.length();
  }
  return size_;
}

void Reassembler::insertToBuffer( uint64_t first_idx, std::string data, bool is_last_substring) {
  if (buffer_.empty()) {
    buffer_.emplace_back(first_idx, std::make_pair(data, is_last_substring));
    return;
  }
  uint64_t idx;
  for(idx=0;idx<buffer_.size();idx++) {
    auto index = buffer_[idx].first;
    auto len = buffer_[idx].second.first.length();
    if (first_idx < index) {
      break;
    }
    if (first_idx == index && data.length() < len) {
      break;
    }
  }

  buffer_.insert(buffer_.begin()+idx, std::make_pair(first_idx, std::make_pair(data, is_last_substring)));

  // 在之前需要得到有序并不重叠的字节流
  // merge the intervals
  std::vector<std::pair<uint64_t, std::pair<std::string, bool>>> newBuffer{};
  for(const auto& item: buffer_) {
    auto n = newBuffer.size();
    if (newBuffer.empty() || newBuffer[n-1].first + newBuffer[n-1].second.first.length() < item.first) {
      newBuffer.emplace_back(item);
    } else {
      auto& last = newBuffer[n-1];
      if (item.first + item.second.first.length() <= last.first + last.second.first.length()) {
        continue;
      }
      if (last.first + last.second.first.length() >= item.first) {
        last.second.first += item.second.first.substr(last.first + last.second.first.length() - item.first);
      }
      if (item.second.second) {
        last.second.second = item.second.second;
      }
    }
  }
  buffer_ = newBuffer;
}

void Reassembler::checkBuffer( Writer& output ) {
  uint64_t firstUnpoppedIndex = 0;
  std::vector<std::pair<uint64_t, std::pair<std::string, bool>>> newBuffer{};
  for (const auto& item:buffer_) {
    firstUnpoppedIndex = output.bytes_pushed();
    auto first_index = item.first;
    auto data = item.second.first;

    if (first_index + data.length() <= firstUnpoppedIndex) {
      continue;
    }

    if (first_index < firstUnpoppedIndex && firstUnpoppedIndex < first_index + data.length()) {
      auto bIdx = firstUnpoppedIndex - first_index;
      auto subStrs = data.substr(bIdx);
      output.push(subStrs);
      if (item.second.second) {
        output.close();
      }
      continue;
    }

    if (firstUnpoppedIndex == first_index) {
      output.push(data);
      if (item.second.second) {
        output.close();
      }
      continue;
    }
    newBuffer.emplace_back(item);
  }

  buffer_ = newBuffer;
}
