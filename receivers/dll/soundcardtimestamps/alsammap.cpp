#include <string>
#include <alsa/asoundlib.h>
#include <stdint.h>
#include <math.h>

#ifndef M_PI
static const double M_Pi = 3.14159265358979323846;
#endif

void check(const std::string & function_name, int error) {
  if (error) { 
    fprintf(stderr, "function %s returned error code %d (%s)\n",
            function_name.c_str(), error, snd_strerror(error));
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

class NarrowingDLL {
  double nominalUpdateRate;
  double initialBandWidth, terminalBandWidth, currentBandWidth;
  double recursiveBandWidthFilterCoefficient;
  double b,c,e2,t1;
  bool reset;

  void updateDllCoefficients() {
    if (reset) {
      currentBandWidth = initialBandWidth;
    } else {
      currentBandWidth = currentBandWidth * recursiveBandWidthFilterCoefficient
        + terminalBandWidth * (1 - recursiveBandWidthFilterCoefficient);
    }
    double omega = 2*M_PI * currentBandWidth / nominalUpdateRate;
    b = sqrt(2) * omega;
    c = omega * omega;
  }

public:
  NarrowingDLL(double nominalUpdateRate,
               double initialBandWidth,
               double terminalBandWidth,
               double bandWidthAdaptationTimeConstant)
    : nominalUpdateRate(nominalUpdateRate),
      initialBandWidth(initialBandWidth),
      terminalBandWidth(terminalBandWidth),
      reset(true)
  {
    double bandWidthAdaptationTimeConstantInUpdates =
      bandWidthAdaptationTimeConstant * nominalUpdateRate;
    recursiveBandWidthFilterCoefficient =
      exp(-1 / bandWidthAdaptationTimeConstantInUpdates);
  }

  std::pair<double,double> operator()(double new_t0) 
  {
    double t0;
    updateDllCoefficients();
    if (reset) {
      t0 = new_t0;
      e2 = 1 / nominalUpdateRate;
      t1 = t0 + e2;
      reset = false;
    } else {
      t0 = t1;
      double e = new_t0 - t0;
      t1 = t0 + b*e + e2;
      e2 = e2 + c*e;
    }
    return std::pair<double,double>(t0,t1);
  }
};


SoundSender::Clock clock_;
class NarrowingDLL dll(double(MULTICAST_SAMPLERATE)/MULTICAST_BLOCK_SIZE,
                       2, 0.01, 5);
SoundSender::Clock::nsec_t analysbuf[20000] = {};              
SoundSender::Clock::nsec_t sample_times[MULTICAST_BLOCK_SIZE];

void save_timestamps() {
  static SoundSender::Clock::nsec_t dll_epoch = clock_.get_nsec();
  static unsigned analysbufindex = 0;
  SoundSender::Clock::nsec_t now = clock_.get_nsec();
  auto filtered = dll((now - dll_epoch) / 1e9);
  double t0 = filtered.first;
  double dT = filtered.second - t0;
  for (unsigned k = 0; k < MULTICAST_BLOCK_SIZE; ++k) {
    sample_times[k] =
      static_cast<SoundSender::Clock::nsec_t>((t0 + k * dT / MULTICAST_BLOCK_SIZE) * 1e9);
    sample_times[k] += dll_epoch;
  }
  analysbuf[analysbufindex++] = sample_times[0];
  analysbufindex %= 20000;
}

inline int16_t sound(bool side, int64_t time) {
  double f = side ? 440.0 : 623.0;
  double frac = (time % 1000000000) / 1e9;
  double phase =  2 * M_PI * f * frac;
  double fine = sin(phase);
  double env = (frac > 0.125) ? 0.0 : sin(2*M_PI*4*frac);
  return static_cast<int16_t>(32000 * fine * env);
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
        *(int16_t*) sample_address = sound(ch,sample_times[k]);
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
  
