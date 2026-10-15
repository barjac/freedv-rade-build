/* FreeDV Bandpass Filter
 *
 * Compile (wide, RADE V1):
 *   gcc -O3 -o bpf bpf.c -lm -lasound
 * Compile (1k, RADE V2):
 *   gcc -O3 -DCUTOFF_LOW=981.0 -DCUTOFF_HIGH=1956.0 -o bpf_1k bpf.c -lm -lasound
 *
 * Plain C, no -march=native, so it builds on x86_64 and aarch64 alike.
 *
 * Cutoffs are the -6 dB points; each sits 50 Hz outside the band occupied by
 * the RADE carriers (rade_src/src/rade_ofdm.c, rade_v2_ofdm.c):
 *   V1: 30 carriers x 50 Hz,   750-2200 Hz, occupied 725-2225 Hz, centre 1475 Hz
 *   V2: 14 carriers x 62.5 Hz, 1062.5-1875 Hz, occupied 1031-1906 Hz, centre 1468.75 Hz
 * The V2 cutoffs match RADE's own V2 receive BPF (rade_rx_v2.c).
 *
 * The filter normally runs inside PipeWire: the program writes the filter
 * as an impulse response (a 48 kHz float WAV) and a filter-chain config to
 * $XDG_RUNTIME_DIR, then runs "pipewire -c" on that config as a child. The
 * filter-chain's convolver does the filtering in PipeWire's real-time data
 * thread, so it can't fall behind the way a separate process reading and
 * writing ALSA can (on a busy Raspberry Pi 4 that dropped blocks of the
 * transmit audio). Stopping the program stops the filter-chain.
 *
 * Without the filter-chain (an old PipeWire, or "--alsa" given), it filters
 * in its own process through PipeWire's ALSA device instead. There, 48 kHz
 * is requested and, as a safety net, the filter is designed at the rate
 * ALSA actually gives, with a warning if that isn't 48 kHz.
 *
 * Either way the PipeWire nodes are named after the program (Fdv-bpf,
 * Fdv-bpf-1k), with mono ports input_MONO and output_MONO.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE 48000
#define CHANNELS 1
#define BUFFER_SIZE 512
// ALSA ring buffer, in frames (~85 ms at 48 kHz). Left to the PipeWire ALSA
// plugin's default it is far bigger, and the filter then passes the audio
// through a backlog (measured ~0.9 s): the transmit audio reached the radio
// that much after PTT.
#define RING_SIZE (8 * BUFFER_SIZE)

// Filter parameters (override with -D at compile time)
#ifndef CUTOFF_LOW
#define CUTOFF_LOW 675.0
#endif
#ifndef CUTOFF_HIGH
#define CUTOFF_HIGH 2275.0
#endif
#define TRANSITION_WIDTH 100.0
#define KAISER_BETA 6.76

// Globals for signal handling
static volatile sig_atomic_t running = 1;
static snd_pcm_t *capture_handle, *playback_handle;

// alsa-lib's blocking read/write just waits again when a signal interrupts
// it, so with no audio flowing (e.g. nothing linked yet) Ctrl+C or kill
// would never stop the filter. snd_pcm_abort() makes the waiting call
// return -EINTR instead.
void signal_handler(int sig) {
    running = 0;
    if (capture_handle)
        snd_pcm_abort(capture_handle);
    if (playback_handle)
        snd_pcm_abort(playback_handle);
}

// Modified Bessel function I0 (for Kaiser window)
double bessel_i0(double x) {
    double sum = 1.0;
    double term = 1.0;
    double x_half_squared = (x / 2.0) * (x / 2.0);

    for (int i = 1; i < 50; i++) {
        term *= x_half_squared / (i * i);
        sum += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}

// Kaiser window function
void kaiser_window(double *window, int n, double beta) {
    double i0_beta = bessel_i0(beta);
    int m = n - 1;

    for (int i = 0; i < n; i++) {
        double arg = beta * sqrt(1.0 - pow(2.0 * i / (double)m - 1.0, 2.0));
        window[i] = bessel_i0(arg) / i0_beta;
    }
}

// Design bandpass FIR filter (float taps for efficiency)
int design_bandpass_filter(float **taps, double fs, double f1, double f2,
                           double transition_width, double beta) {
    int num_taps = (int)(4.0 * fs / transition_width);
    if (num_taps % 2 == 0) num_taps++;

    if (num_taps < 51) num_taps = 51;
    if (num_taps > 10001) num_taps = 10001;

    *taps = (float *)malloc(num_taps * sizeof(float));
    if (!*taps) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }

    double wc1 = 2.0 * M_PI * f1 / fs;
    double wc2 = 2.0 * M_PI * f2 / fs;
    int m = (num_taps - 1) / 2;

    double *temp_taps = (double *)malloc(num_taps * sizeof(double));
    double *window = (double *)malloc(num_taps * sizeof(double));

    if (!temp_taps || !window) {
        fprintf(stderr, "Memory allocation failed\n");
        free(*taps);
        return -1;
    }

    // Design ideal bandpass filter
    for (int n = 0; n < num_taps; n++) {
        int idx = n - m;
        if (idx == 0) {
            temp_taps[n] = (wc2 - wc1) / M_PI;
        } else {
            temp_taps[n] = (sin(wc2 * idx) - sin(wc1 * idx)) / (M_PI * idx);
        }
    }

    // Apply Kaiser window
    kaiser_window(window, num_taps, beta);

    // Store as float taps
    for (int n = 0; n < num_taps; n++) {
        (*taps)[n] = (float)(temp_taps[n] * window[n]);
    }

    free(window);
    free(temp_taps);

    printf("Designed filter with %d taps\n", num_taps);
    printf("Filter: %.0f-%.0f Hz, transition: %.0f Hz\n", f1, f2, transition_width);

    return num_taps;
}

// FIR filter context structure
typedef struct {
    float *taps;
    float *history;
    int num_taps;
    int pos;
} fir_filter_t;

// Initialize FIR filter
fir_filter_t* fir_filter_create(float *taps, int num_taps) {
    fir_filter_t *fir = (fir_filter_t *)malloc(sizeof(fir_filter_t));
    if (!fir) return NULL;

    fir->taps = taps;
    fir->num_taps = num_taps;
    fir->pos = 0;

    // Double-sized history buffer for circular buffer trick
    fir->history = (float *)calloc(num_taps * 2, sizeof(float));
    if (!fir->history) {
        free(fir);
        return NULL;
    }

    return fir;
}

// Process samples using circular buffer
void fir_filter_process(fir_filter_t *fir, float *input, float *output, int length) {
    int num_taps = fir->num_taps;
    float *taps = fir->taps;
    float *history = fir->history;
    int pos = fir->pos;

    for (int i = 0; i < length; i++) {
        // Store in both halves of circular buffer
        history[pos] = input[i];
        history[pos + num_taps] = input[i];

        // Convolve - no wrap-around needed
        float sum = 0.0f;
        float *h = &history[pos];

        for (int j = 0; j < num_taps; j++) {
            sum += taps[j] * h[j];
        }

        output[i] = sum;

        pos--;
        if (pos < 0) pos = num_taps - 1;
    }

    fir->pos = pos;
}

void fir_filter_destroy(fir_filter_t *fir) {
    if (fir) {
        free(fir->history);
        free(fir);
    }
}

// Open PipeWire's own ALSA device, so the filter is a PipeWire node named by
// PIPEWIRE_PROPS whatever ALSA's "default" is (on Ubuntu 22.04 that is
// PulseAudio's plugin, even when PipeWire is running). Falls back to
// "default" where the "pipewire" device isn't defined.
// Keep the period at the processing block size and the ring buffer small,
// so no backlog can build up between FreeDV and the radio.
static void set_small_buffer(snd_pcm_t *handle, snd_pcm_hw_params_t *hw_params)
{
    snd_pcm_uframes_t period = BUFFER_SIZE, ring = RING_SIZE;
    snd_pcm_hw_params_set_period_size_near(handle, hw_params, &period, 0);
    snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &ring);
}

static void print_buffer(const char *what, snd_pcm_t *handle, unsigned int rate)
{
    snd_pcm_uframes_t ring = 0, period = 0;
    snd_pcm_get_params(handle, &ring, &period);
    printf("%s buffer: %lu frames (%.0f ms), period %lu\n",
           what, (unsigned long)ring, ring * 1000.0 / rate, (unsigned long)period);
}

static int open_pcm(snd_pcm_t **handle, snd_pcm_stream_t stream)
{
    int err = snd_pcm_open(handle, "pipewire", stream, 0);
    if (err < 0)
        err = snd_pcm_open(handle, "default", stream, 0);
    return err;
}

/* ── Filter-chain mode ───────────────────────────────────────────────── */

static volatile pid_t chain_pid;

static void chain_signal_handler(int sig) {
    if (chain_pid > 0)
        kill(chain_pid, sig);
}

static void put_le(FILE *f, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; i++)
        fputc((v >> (8 * i)) & 0xff, f);
}

// Mono 32-bit float WAV
static int write_ir_wav(const char *path, const float *taps, int n, int rate) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t data = (uint32_t)n * 4;
    fputs("RIFF", f); put_le(f, 4 + 26 + 12 + 8 + data, 4); fputs("WAVE", f);
    fputs("fmt ", f); put_le(f, 18, 4);
    put_le(f, 3, 2);                // WAVE_FORMAT_IEEE_FLOAT
    put_le(f, 1, 2);                // channels
    put_le(f, rate, 4);
    put_le(f, rate * 4, 4);         // bytes per second
    put_le(f, 4, 2);                // block align
    put_le(f, 32, 2);               // bits per sample
    put_le(f, 0, 2);                // no extension
    fputs("fact", f); put_le(f, 4, 4); put_le(f, n, 4);
    fputs("data", f); put_le(f, data, 4);
    fwrite(taps, sizeof(float), n, f);
    return fclose(f) == 0 ? 0 : -1;
}

static int write_chain_conf(const char *path, const char *prog, const char *wav) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f,
        "# Written by %s each time it starts.\n"
        "context.properties = { log.level = 0 }\n"
        "context.spa-libs = {\n"
        "    audio.convert.* = audioconvert/libspa-audioconvert\n"
        "    support.*       = support/libspa-support\n"
        "}\n"
        "context.modules = [\n"
        "    { name = libpipewire-module-rt flags = [ ifexists nofail ] }\n"
        "    { name = libpipewire-module-protocol-native }\n"
        "    { name = libpipewire-module-client-node }\n"
        "    { name = libpipewire-module-adapter }\n"
        "    { name = libpipewire-module-filter-chain\n"
        "        args = {\n"
        "            node.description = \"%s %.0f-%.0fHz\"\n"
        "            media.name       = \"%.0f-%.0fHz\"\n"
        "            filter.graph = {\n"
        "                nodes = [\n"
        "                    { type = builtin name = bpf label = convolver\n"
        "                      config = { filename = \"%s\" } }\n"
        "                ]\n"
        "            }\n"
        "            audio.rate     = %d\n"
        "            audio.channels = 1\n"
        "            audio.position = [ MONO ]\n"
        "            capture.props  = { node.name = \"%s\" node.autoconnect = false }\n"
        "            playback.props = { node.name = \"%s\" node.autoconnect = false }\n"
        "        }\n"
        "    }\n"
        "]\n",
        prog, prog, CUTOFF_LOW, CUTOFF_HIGH, CUTOFF_LOW, CUTOFF_HIGH,
        wav, SAMPLE_RATE, prog, prog);
    return fclose(f) == 0 ? 0 : -1;
}

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Runs the filter as a PipeWire filter-chain until stopped. Returns 0 when
// it ran and was stopped, -1 if it couldn't start (use the ALSA filter).
static int run_filter_chain(const char *prog) {
    const char *dir = getenv("XDG_RUNTIME_DIR");
    if (!dir || !*dir) dir = "/tmp";
    char wav[512], conf[512];
    snprintf(wav, sizeof wav, "%s/%s.wav", dir, prog);
    snprintf(conf, sizeof conf, "%s/%s.conf", dir, prog);

    float *taps;
    int n = design_bandpass_filter(&taps, SAMPLE_RATE, CUTOFF_LOW, CUTOFF_HIGH,
                                   TRANSITION_WIDTH, KAISER_BETA);
    if (n < 0) return -1;
    int bad = write_ir_wav(wav, taps, n, SAMPLE_RATE) || write_chain_conf(conf, prog, wav);
    free(taps);
    if (bad) {
        fprintf(stderr, "Cannot write %s or %s: %s\n", wav, conf, strerror(errno));
        return -1;
    }

    signal(SIGINT, chain_signal_handler);
    signal(SIGTERM, chain_signal_handler);
    fflush(stdout);
    double start = now_s();
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        // Stop the filter-chain with this program, even if it is killed
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == 1) _exit(0);
        execlp("pipewire", "pipewire", "-c", conf, (char *)NULL);
        _exit(127);
    }
    chain_pid = pid;
    printf("Filter running in a PipeWire filter-chain (%s)\n", conf);
    fflush(stdout);

    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
    chain_pid = 0;
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    // Gone within 3s and not because it was told to stop: it never started
    if (now_s() - start < 3.0 && !(WIFSIGNALED(status)
            && (WTERMSIG(status) == SIGTERM || WTERMSIG(status) == SIGINT))) {
        fprintf(stderr, "The PipeWire filter-chain did not start - "
                        "filtering through ALSA instead\n");
        return -1;
    }
    return 0;
}

/* ── ALSA mode ───────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    snd_pcm_hw_params_t *hw_params;
    float *filter_taps;
    int num_taps;
    float *input_buffer, *output_buffer;
    fir_filter_t *fir;
    int err;

    // The node is named after the program (Fdv-bpf, Fdv-bpf-1k): PipeWire
    // shows the program name too, and qpwgraph then shows the name only
    // once. Its label is the passband. Values are quoted, or PipeWire stops
    // reading the list there. No auto-connect - prevents PipeWire from
    // making unwanted connections when the filter starts.
    const char *prog = strrchr(argv[0], '/');
    prog = prog ? prog + 1 : argv[0];
    char props[256];
    snprintf(props, sizeof props,
             "node.name=\"%s\" media.name=\"%.0f-%.0fHz\" node.autoconnect=false",
             prog, CUTOFF_LOW, CUTOFF_HIGH);

    printf("FreeDV Bandpass Filter\n");
    printf("======================\n");

    if (!(argc > 1 && strcmp(argv[1], "--alsa") == 0)
            && run_filter_chain(prog) == 0)
        return 0;

    setenv("PIPEWIRE_PROPS", props, 0);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Allocate buffers
    input_buffer = (float *)calloc(BUFFER_SIZE, sizeof(float));
    output_buffer = (float *)calloc(BUFFER_SIZE, sizeof(float));

    if (!input_buffer || !output_buffer) {
        fprintf(stderr, "Failed to allocate audio buffers\n");
        return 1;
    }

    // Open ALSA capture device
    if ((err = open_pcm(&capture_handle, SND_PCM_STREAM_CAPTURE)) < 0) {
        fprintf(stderr, "Cannot open audio capture device: %s\n",
                snd_strerror(err));
        return 1;
    }

    // Configure capture
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);
    snd_pcm_hw_params_set_access(capture_handle, hw_params,
                                  SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, hw_params,
                                  SND_PCM_FORMAT_FLOAT);
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, CHANNELS);
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0);
    set_small_buffer(capture_handle, hw_params);
    snd_pcm_hw_params(capture_handle, hw_params);

    printf("Capture rate: %d Hz\n", rate);
    unsigned int capture_rate = rate;
    if (capture_rate != SAMPLE_RATE)
        fprintf(stderr, "Warning: got %u Hz instead of %d Hz, designing filter for %u Hz\n",
                capture_rate, SAMPLE_RATE, capture_rate);

    // Open ALSA playback device
    if ((err = open_pcm(&playback_handle, SND_PCM_STREAM_PLAYBACK)) < 0) {
        fprintf(stderr, "Cannot open audio playback device: %s\n",
                snd_strerror(err));
        snd_pcm_close(capture_handle);
        return 1;
    }

    // Configure playback
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(playback_handle, hw_params);
    snd_pcm_hw_params_set_access(playback_handle, hw_params,
                                  SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback_handle, hw_params,
                                  SND_PCM_FORMAT_FLOAT);
    snd_pcm_hw_params_set_channels(playback_handle, hw_params, CHANNELS);
    rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0);
    set_small_buffer(playback_handle, hw_params);
    snd_pcm_hw_params(playback_handle, hw_params);

    printf("Playback rate: %d Hz\n", rate);
    print_buffer("Capture", capture_handle, capture_rate);
    print_buffer("Playback", playback_handle, rate);
    if (rate != capture_rate)
        fprintf(stderr, "Warning: playback rate %u Hz differs from capture rate %u Hz\n",
                rate, capture_rate);

    // Design filter at the negotiated capture rate
    num_taps = design_bandpass_filter(&filter_taps, capture_rate,
                                      CUTOFF_LOW, CUTOFF_HIGH,
                                      TRANSITION_WIDTH, KAISER_BETA);
    if (num_taps < 0) {
        fprintf(stderr, "Failed to design filter\n");
        return 1;
    }

    // Create filter
    fir = fir_filter_create(filter_taps, num_taps);
    if (!fir) {
        fprintf(stderr, "Failed to create filter\n");
        return 1;
    }

    // Prepare devices
    snd_pcm_prepare(capture_handle);
    snd_pcm_prepare(playback_handle);

    printf("\nFilter running - Press Ctrl+C to stop\n");
    fflush(stdout);

    // Main processing loop
    while (running) {
        // Read audio
        if ((err = snd_pcm_readi(capture_handle, input_buffer,
                                 BUFFER_SIZE)) != BUFFER_SIZE) {
            if (err < 0) {
                if (!running)
                    break;
                fprintf(stderr, "Read error: %s\n", snd_strerror(err));
                snd_pcm_prepare(capture_handle);
                usleep(10000);  // Prevent CPU spin
                continue;
            }
        }

        // Apply filter
        fir_filter_process(fir, input_buffer, output_buffer, BUFFER_SIZE);

        // Write audio
        if ((err = snd_pcm_writei(playback_handle, output_buffer,
                                  BUFFER_SIZE)) != BUFFER_SIZE) {
            if (err < 0) {
                if (!running)
                    break;
                fprintf(stderr, "Write error: %s\n", snd_strerror(err));
                snd_pcm_prepare(playback_handle);
                usleep(10000);  // Prevent CPU spin
                continue;
            }
        }
    }

    // Cleanup
    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    fir_filter_destroy(fir);
    free(filter_taps);
    free(input_buffer);
    free(output_buffer);

    printf("\nStopped\n");
    return 0;
}
