#include "news/byte_buffer.h"

#include <algorithm>

namespace news {

ByteBuffer::ByteBuffer(const std::size_t capacity)
    : capacity_(capacity) {
  storage_.reserve(capacity_);
}

bool ByteBuffer::Append(const std::vector<std::byte>& bytes) {
  if (bytes.size() > available()) {
    return false;
  }

  if (storage_.size() + bytes.size() > capacity_) {
    Compact();
  }

  storage_.insert(storage_.end(), bytes.begin(), bytes.end());
  return true;
}

void ByteBuffer::Consume(const std::size_t count) {
  read_offset_ += std::min(count, size());
  if (read_offset_ == storage_.size()) {
    Clear();
  }
}

void ByteBuffer::Clear() {
  storage_.clear();
  read_offset_ = 0;
}

const std::vector<std::byte>& ByteBuffer::data() const {
  return storage_;
}

std::size_t ByteBuffer::read_offset() const {
  return read_offset_;
}

std::size_t ByteBuffer::size() const {
  return storage_.size() - read_offset_;
}

std::size_t ByteBuffer::capacity() const {
  return capacity_;
}

std::size_t ByteBuffer::available() const {
  return capacity_ - size();
}

bool ByteBuffer::empty() const {
  return size() == 0;
}

void ByteBuffer::Compact() {
  storage_.erase(
      storage_.begin(),
      storage_.begin() + static_cast<std::ptrdiff_t>(read_offset_));
  read_offset_ = 0;
}

}  // namespace news
