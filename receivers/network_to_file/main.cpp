#include <string>
#include <stdio.h>
#include <asio.hpp>


#define MULTICAST_IP "226.125.44.170"
#define MULTICAST_PORT 48221U
#define BILLION static_cast<const SoundSender::Clock::nsec_t>(1000000000)
#define MULTICAST_CHANNELS 2U
#define MULTICAST_SAMPLERATE 48000U
#define MULTICAST_BLOCK_SIZE 256U

namespace SoundSender {
  // Clock declaration
  class Clock {
  public:
    typedef int64_t nsec_t;
    nsec_t get_nsec() const;
  };

  // Network packet declaration
  struct NetworkPacket {
    char magic[4];
    uint32_t protocol_version;
    uint32_t stream_id;
    uint32_t packet_number;
    Clock::nsec_t audible_time;
    int16_t audio[MULTICAST_BLOCK_SIZE * MULTICAST_CHANNELS];
  };

  // Clock implementation
  Clock::nsec_t Clock::get_nsec() const {
    nsec_t nsec = 0;
    struct timespec ts = {0,0};
    clock_gettime(CLOCK_REALTIME, &ts);
    nsec = ts.tv_sec;
    nsec *= 1000000000;
    nsec += ts.tv_nsec;
    return nsec;
  }
}

int main(int argc, char * const * argv)
{
  asio::io_service asio_service;
  if (argc < 2) {
    fprintf(stderr, "Need command line parameter specifying IP address of local"
            " network interface to use for receiving Multicast UDP packages\n");
    exit(1);
  }
  try {
    asio::ip::udp::endpoint multicast_receiver_endpoint
      (asio::ip::address::from_string(MULTICAST_IP), MULTICAST_PORT);
    
    asio::ip::udp::socket multicast_receiver_socket(asio_service);
    multicast_receiver_socket.open(multicast_receiver_endpoint.protocol());
    multicast_receiver_socket.set_option(asio::ip::udp::socket::reuse_address(true));
    multicast_receiver_socket.bind(multicast_receiver_endpoint);
    multicast_receiver_socket.set_option
      (asio::ip::multicast::join_group
       (asio::ip::address::from_string(MULTICAST_IP).to_v4(),
        asio::ip::address::from_string(argv[1]).to_v4()
        )
       );
    SoundSender::NetworkPacket p;
    asio::mutable_buffers_1 buf(&p, sizeof(p));
    for(;;) {
      multicast_receiver_socket.receive(buf);
      printf("- \n");
      printf("   magic: ['%c','%c','%c','%c']\n",
             p.magic[0],p.magic[1],p.magic[2],p.magic[3]);
      printf("   protocol_version: %u\n", p.protocol_version);
      printf("   stream_id: %u\n", p.stream_id);
      printf("   packet_number: %u\n", p.packet_number);
      printf("   audible_time: %lld\n", (long long)p.audible_time);
      printf("   audio: [");
      unsigned i;
      for (i = 0; i < (MULTICAST_BLOCK_SIZE * MULTICAST_CHANNELS-1); ++i)
        printf("%hd,", p.audio[i]);
      printf("%hd]\n", p.audio[i]);
    }
  } catch (std::exception & e) {
    fprintf(stderr, "Caught exception %s\n", e.what());
    return 1;
  }
  return 0;
}

// Local Variables:
// c-basic-offset: 2
// indent-tabs-mode: nil
// compile-command: "g++ --std=c++1y -g -o network-to-file main.cpp"
// End:
