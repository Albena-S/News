#ifndef NEWS_INCLUDE_NEWS_BYTE_BUFFER_H_
#define NEWS_INCLUDE_NEWS_BYTE_BUFFER_H_

#include <cstddef>
#include <vector>

namespace news {

// A bounded byte buffer backed by std::vector. Consuming bytes advances a read
// offset instead of moving the remaining bytes. Storage is compacted only when
// an append needs to reuse consumed space.
class ByteBuffer {
 public:
  ByteBuffer(std::size_t capacity);

  bool Append(const std::vector<std::byte>& bytes);
  void Consume(std::size_t count);
  void Clear();

  const std::vector<std::byte>& data() const;
  std::size_t read_offset() const;
  std::size_t size() const;
  std::size_t capacity() const;
  std::size_t available() const;
  bool empty() const;

 private:
  void Compact();

  std::vector<std::byte> storage_;
  std::size_t capacity_;
  std::size_t read_offset_{0};
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_BYTE_BUFFER_H_
