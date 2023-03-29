#include <flute/common/LogStream.h>

int main() {
  {
    printf("================LogStream Test================\n");
    flute::LogStream os;
    const flute::LogBuffer& buffer = os.m_buffer;
    os << 123;
    printf("Data: %s\n", buffer.to_string().c_str());
    os << "@";
    printf("Data: %s\n", buffer.to_string().c_str());
    os << 456;
    printf("Data: %s\n", buffer.to_string().c_str());
    os << 0x64 << "?";
    printf("Data: %s\n", buffer.to_string().c_str());
  }
  return 0;
}