#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define SAMPLE_SIZE 16
#define PERIOD_SIZE 1024
#define BUFFER_SIZE (PERIOD_SIZE * 4)

static int running = 1;

void signal_handler(int sig) {
    running = 0;
}

snd_pcm_t* setup_mmap_pcm(const char* device_name) {
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    int err;
    
    // 1. 打开PCM设备，支持MMAP
    err = snd_pcm_open(&pcm, device_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot open PCM device %s: %s\n", 
                device_name, snd_strerror(err));
        return NULL;
    }
    
    // 2. 配置硬件参数
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(pcm, hw_params);
    
    // 设置访问模式为MMAP交错访问
    snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    
    // 设置音频格式
    snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, hw_params, CHANNELS);
    
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, 0);
    
    // 设置周期大小和缓冲区大小
    snd_pcm_uframes_t period_size = PERIOD_SIZE;
    snd_pcm_uframes_t buffer_size = BUFFER_SIZE;
    
    snd_pcm_hw_params_set_period_size_near(pcm, hw_params, &period_size, 0);
    snd_pcm_hw_params_set_buffer_size_near(pcm, hw_params, &buffer_size);
    
    // 应用硬件参数
    if ((err = snd_pcm_hw_params(pcm, hw_params)) < 0) {
        fprintf(stderr, "Cannot set hw params: %s\n", snd_strerror(err));
        snd_pcm_close(pcm);
        return NULL;
    }
    
    // 3. 配置软件参数
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(pcm, sw_params);
    
    // 设置开始阈值
    snd_pcm_sw_params_set_start_threshold(pcm, sw_params, period_size);
    snd_pcm_sw_params_set_avail_min(pcm, sw_params, period_size);
    
    if ((err = snd_pcm_sw_params(pcm, sw_params)) < 0) {
        fprintf(stderr, "Cannot set sw params: %s\n", snd_strerror(err));
        snd_pcm_close(pcm);
        return NULL;
    }
    
    printf("MMAP PCM setup complete:\n");
    printf("  Rate: %d Hz\n", rate);
    printf("  Channels: %d\n", CHANNELS);
    printf("  Period size: %lu frames\n", period_size);
    printf("  Buffer size: %lu frames\n", buffer_size);
    
    return pcm;
}

void generate_sine_wave(int16_t *buffer, int frames, double *phase, double frequency) {
    int i;
    double phase_increment = 2.0 * M_PI * frequency / SAMPLE_RATE;
    
    for (i = 0; i < frames * CHANNELS; i += CHANNELS) {
        int16_t sample = (int16_t)(sin(*phase) * 16000);
        buffer[i] = sample;     // 左声道
        buffer[i + 1] = sample; // 右声道
        *phase += phase_increment;
        if (*phase >= 2.0 * M_PI) {
            *phase -= 2.0 * M_PI;
        }
    }
}

int mmap_audio_playback(snd_pcm_t *pcm) {
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t offset, frames;
    int err;
    double phase = 0.0;
    double frequency = 440.0; // A4音符
    
    printf("Starting MMAP audio playback...\n");
    
    while (running) {
        // 1. 等待PCM设备准备好
        err = snd_pcm_wait(pcm, 1000);
        if (err < 0) {
            if (err == -EAGAIN) continue;
            fprintf(stderr, "Poll failed: %s\n", snd_strerror(err));
            break;
        }
        
        // 2. 获取可用的帧数
        snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm);
        if (avail < 0) {
            if (avail == -EPIPE) {
                fprintf(stderr, "Underrun occurred\n");
                snd_pcm_prepare(pcm);
                continue;
            }
            fprintf(stderr, "avail_update failed: %s\n", snd_strerror(avail));
            break;
        }
        
        if (avail < PERIOD_SIZE) {
            continue; // 等待更多空间
        }
        
        frames = PERIOD_SIZE;
        
        // 3. 开始MMAP访问
        err = snd_pcm_mmap_begin(pcm, &areas, &offset, &frames);
        if (err < 0) {
            fprintf(stderr, "mmap_begin failed: %s\n", snd_strerror(err));
            break;
        }
        
        // 4. 直接在MMAP区域中写入音频数据
        int16_t *samples = (int16_t *)((char *)areas[0].addr + 
                                      (areas[0].first + offset * areas[0].step) / 8);
        
        // 生成正弦波数据直接写入DMA缓冲区
        generate_sine_wave(samples, frames, &phase, frequency);
        
        // 5. 提交MMAP数据
        snd_pcm_sframes_t committed = snd_pcm_mmap_commit(pcm, offset, frames);
        if (committed < 0) {
            fprintf(stderr, "mmap_commit failed: %s\n", snd_strerror(committed));
            break;
        }
        
        if (committed != (snd_pcm_sframes_t)frames) {
            fprintf(stderr, "Short commit: %ld expected %lu\n", committed, frames);
        }
        
        // 6. 如果PCM还没开始，现在启动它
        if (snd_pcm_state(pcm) == SND_PCM_STATE_PREPARED) {
            err = snd_pcm_start(pcm);
            if (err < 0) {
                fprintf(stderr, "Start failed: %s\n", snd_strerror(err));
                break;
            }
            printf("PCM playback started\n");
        }
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    const char *device = argc > 1 ? argv[1] : "hw:0,0";
    snd_pcm_t *pcm;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("MMAP Audio Playback Example\n");
    printf("Device: %s\n", device);
    printf("Press Ctrl+C to stop...\n\n");
    
    pcm = setup_mmap_pcm(device);
    if (!pcm) {
        return 1;
    }
    
    mmap_audio_playback(pcm);
    
    snd_pcm_close(pcm);
    printf("\nPlayback stopped\n");
    
    return 0;
} 