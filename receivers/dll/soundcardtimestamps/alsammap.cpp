#include <string>
#include <alsa/asoundlib.h>

void check(const std::string & function_name, int error) {
  if (error) { 
    fprintf(stderr, "function %s returned error code %d\n",
            function_name.c_str(), error);
    exit(1);
  }
}

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

#define NSTAMPS 11111

SoundSender::Clock clock_;
SoundSender::Clock::nsec_t timestamps[NSTAMPS] = {};
int frame_count_min[NSTAMPS]={}, frame_count_max[NSTAMPS]={};
size_t tsindex = 0;

void save_timestamps() {
  timestamps[tsindex++] = clock_.get_nsec();
  if (tsindex == sizeof(timestamps)/sizeof(timestamps[0])) {
    int fd = open("/tmp/timestamps.mmap", O_WRONLY | O_CREAT | O_TRUNC);
    write(fd, timestamps, sizeof(timestamps));
    write(fd, ::frame_count_min, sizeof(::frame_count_min));
    write(fd, ::frame_count_max, sizeof(::frame_count_max));
    close(fd);
  }
}

int main(int argc, char ** argv) {
  const char * alsa_playback_device = "hw:0,0";
  snd_pcm_t * pcm = 0;
  check("snd_pcm_open",
        snd_pcm_open(&pcm, alsa_playback_device, SND_PCM_STREAM_PLAYBACK, 0));
  check("snd_pcm_open", !pcm);
  snd_pcm_hw_params_t * hw_params = 0;
  check("snd_pcm_hw_params_malloc", snd_pcm_hw_params_malloc(&hw_params));
  check("snd_pcm_hw_params_malloc", !hw_params);
  check("snd_pcm_hw_params_any", snd_pcm_hw_params_any(pcm, hw_params));
  check("snd_pcm_hw_params_set_access", 
        snd_pcm_hw_params_set_access(pcm, hw_params,
                                     SND_PCM_ACCESS_MMAP_INTERLEAVED)); 
  check("snd_pcm_hw_params_set_periods_integer",
        snd_pcm_hw_params_set_periods_integer(pcm, hw_params));
  check("snd_pcm_hw_params_set_format",
        snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE));
  check("snd_pcm_hw_params_set_channels",
        snd_pcm_hw_params_set_channels(pcm, hw_params, 2U));
  check("snd_pcm_hw_params_set_rate",
        snd_pcm_hw_params_set_rate(pcm, hw_params, 48000U, 0));
  check("snd_pcm_hw_params_set_period_size",
        snd_pcm_hw_params_set_period_size(pcm, hw_params, 256U, 0));
  check("snd_pcm_hw_params_set_periods",
        snd_pcm_hw_params_set_periods(pcm, hw_params, 2U, 0));
  check("snd_pcm_hw_params", snd_pcm_hw_params(pcm, hw_params));
  
  snd_pcm_sw_params_t * sw_params = 0;
  check("snd_pcm_sw_params_malloc", snd_pcm_sw_params_malloc(&sw_params));
  check("snd_pcm_sw_params_malloc", !sw_params);
  check("snd_pcm_sw_params_current", snd_pcm_sw_params_current(pcm, sw_params));
  snd_pcm_tstamp_t tstamp;
  check("snd_pcm_sw_params_get_tstamp_mode",
        snd_pcm_sw_params_get_tstamp_mode(sw_params, &tstamp));
  printf("timestamp mode = %d\n", tstamp);
  check("snd_pcm_sw_params_set_tstamp_mode",
        snd_pcm_sw_params_set_tstamp_mode(pcm, sw_params, SND_PCM_TSTAMP_MMAP));
  snd_pcm_uframes_t avail_min = -1;
  check("snd_pcm_sw_params_get_avail_min",
        snd_pcm_sw_params_get_avail_min(sw_params, &avail_min));
  printf("avail_min = %lu\n", avail_min);
  check("snd_pcm_sw_params", snd_pcm_sw_params(pcm, sw_params));

  snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm);
  if (avail != 2*256) check("snd_pcm_avail_update", avail ? avail : 777);
  int started = 0;
  // fill buffers with silence
  while (true) {
    save_timestamps();
    snd_pcm_uframes_t offset = 0, frames = 256;
    const snd_pcm_channel_area_t * areas = 0;
    avail = snd_pcm_avail_update(pcm);
    check("snd_pcm_mmap_begin",
          snd_pcm_mmap_begin(pcm, &areas, &offset, &frames));
    for (unsigned ch = 0; ch < 2U; ++ch) {
      for (unsigned k = 0; k < frames; ++k) {
        char * sample_address = (char*)areas[ch].addr;
        sample_address += (areas[ch].first + (k + offset) * areas[ch].step) / 8;
        *(int16_t*) sample_address = 0;
      }
    }
    int err = snd_pcm_mmap_commit (pcm, offset, frames);
    check("snd_pcm_mmap_commit", (err < 0) ? err : 0);
    if (started++ == 0) snd_pcm_start(pcm);
    //printf("waiting...\n");
    err = snd_pcm_wait(pcm, -1);
    //printf("wait returned\n");
    if (err < 0)
      check("snd_pcm_wait", err);
  }
  
  
  return 0;
}
  
