/* 音频DMA驱动配置示例 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

// DMA缓冲区大小配置
#define DMA_BUFFER_SIZE     (64 * 1024)  // 64KB DMA缓冲区
#define DMA_PERIOD_SIZE     (4 * 1024)   // 4KB 周期大小
#define DMA_PERIODS         (DMA_BUFFER_SIZE / DMA_PERIOD_SIZE)

struct audio_dma_data {
    void __iomem *i2s_base;      // I2S寄存器基址
    dma_addr_t dma_addr;         // DMA物理地址
    void *dma_area;              // DMA虚拟地址
    size_t buffer_size;          // 缓冲区大小
    size_t period_size;          // 周期大小
    unsigned int periods;        // 周期数量
    
    // DMA控制器配置
    struct dma_chan *dma_chan;   // DMA通道
    struct dma_slave_config dma_config;
};

/* 1. DMA缓冲区分配 */
static int allocate_dma_buffer(struct snd_pcm_substream *substream) {
    struct audio_dma_data *priv = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;
    
    // 分配连续的DMA内存区域
    priv->dma_area = dma_alloc_coherent(substream->pcm->card->dev,
                                       DMA_BUFFER_SIZE,
                                       &priv->dma_addr,
                                       GFP_KERNEL);
    
    if (!priv->dma_area) {
        pr_err("Failed to allocate DMA buffer\n");
        return -ENOMEM;
    }
    
    // 配置PCM运行时参数
    runtime->dma_addr = priv->dma_addr;
    runtime->dma_area = priv->dma_area;
    runtime->dma_bytes = DMA_BUFFER_SIZE;
    
    priv->buffer_size = DMA_BUFFER_SIZE;
    priv->period_size = DMA_PERIOD_SIZE;
    priv->periods = DMA_PERIODS;
    
    pr_info("DMA buffer allocated: phys=0x%08x, virt=%p, size=%zu\n",
            (u32)priv->dma_addr, priv->dma_area, priv->buffer_size);
    
    return 0;
}

/* 2. DMA传输配置 */
static int configure_dma_transfer(struct audio_dma_data *priv) {
    struct dma_slave_config *config = &priv->dma_config;
    
    // 配置DMA传输参数
    config->direction = DMA_MEM_TO_DEV;          // 内存到设备
    config->dst_addr = (dma_addr_t)(priv->i2s_base + I2S_TX_FIFO_REG); // I2S TX FIFO地址
    config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES; // 32位数据宽度
    config->dst_maxburst = 4;                    // 每次传输4个32位字
    config->device_fc = false;                   // DMA控制器控制流控
    
    // 应用DMA配置
    if (dmaengine_slave_config(priv->dma_chan, config) < 0) {
        pr_err("Failed to configure DMA channel\n");
        return -EINVAL;
    }
    
    return 0;
}

/* 3. 启动DMA传输 */
static int start_dma_transfer(struct audio_dma_data *priv) {
    struct dma_async_tx_descriptor *desc;
    dma_cookie_t cookie;
    
    // 准备循环DMA传输描述符
    desc = dmaengine_prep_dma_cyclic(priv->dma_chan,
                                    priv->dma_addr,      // DMA缓冲区物理地址
                                    priv->buffer_size,   // 总缓冲区大小
                                    priv->period_size,   // 每个周期大小
                                    DMA_MEM_TO_DEV,      // 传输方向
                                    DMA_PREP_INTERRUPT); // 周期结束时产生中断
    
    if (!desc) {
        pr_err("Failed to prepare DMA descriptor\n");
        return -EINVAL;
    }
    
    // 设置DMA完成回调
    desc->callback = dma_transfer_complete_callback;
    desc->callback_param = priv;
    
    // 提交并启动DMA传输
    cookie = dmaengine_submit(desc);
    if (dma_submit_error(cookie)) {
        pr_err("Failed to submit DMA transfer\n");
        return -EINVAL;
    }
    
    dma_async_issue_pending(priv->dma_chan);
    
    pr_info("DMA transfer started\n");
    return 0;
}

/* 4. DMA传输完成中断回调 */
static void dma_transfer_complete_callback(void *data) {
    struct audio_dma_data *priv = data;
    
    // 通知ALSA子系统一个周期完成
    // 这会触发应用程序的下一次mmap写入
    snd_pcm_period_elapsed(priv->substream);
}

/* 5. I2S硬件配置 */
static int configure_i2s_interface(struct audio_dma_data *priv) {
    void __iomem *i2s_base = priv->i2s_base;
    u32 reg_val;
    
    // 配置I2S格式: I2S标准，16位数据，立体声
    reg_val = I2S_FMT_I2S | I2S_WDTH_16 | I2S_MODE_STEREO;
    writel(reg_val, i2s_base + I2S_FORMAT_REG);
    
    // 配置采样率分频器 (48kHz @ 12.288MHz MCLK)
    reg_val = (12288000 / (48000 * 64)) - 1;  // 64 = 32bits * 2channels
    writel(reg_val, i2s_base + I2S_CLKDIV_REG);
    
    // 配置FIFO阈值 - 当FIFO少于4个字时请求DMA
    writel(4, i2s_base + I2S_TXFIFO_THRESHOLD_REG);
    
    // 启用I2S DMA请求
    reg_val = readl(i2s_base + I2S_CTRL_REG);
    reg_val |= I2S_TXDMA_EN | I2S_ENABLE;
    writel(reg_val, i2s_base + I2S_CTRL_REG);
    
    pr_info("I2S interface configured for DMA\n");
    return 0;
}

/* 6. 功放控制接口 */
static int configure_amplifier(struct audio_dma_data *priv) {
    // 通过GPIO或I2C配置功放芯片
    // 示例：使用GPIO使能功放
    gpio_set_value(AMP_ENABLE_GPIO, 1);
    
    // 示例：通过I2C配置功放增益
    // amp_i2c_write(AMP_GAIN_REG, 0x80); // 0dB增益
    
    pr_info("Audio amplifier enabled\n");
    return 0;
}

/* MMAP支持的PCM操作函数 */
static struct snd_pcm_ops audio_pcm_ops = {
    .open = audio_pcm_open,
    .close = audio_pcm_close,
    .ioctl = snd_pcm_lib_ioctl,
    .hw_params = audio_pcm_hw_params,
    .hw_free = audio_pcm_hw_free,
    .prepare = audio_pcm_prepare,
    .trigger = audio_pcm_trigger,
    .pointer = audio_pcm_pointer,
    .mmap = snd_pcm_lib_mmap_iomem,  // 启用MMAP支持
};

/* 硬件约束定义 - 支持MMAP */
static struct snd_pcm_hardware audio_pcm_hardware = {
    .info = SNDRV_PCM_INFO_MMAP |
            SNDRV_PCM_INFO_MMAP_VALID |
            SNDRV_PCM_INFO_INTERLEAVED |
            SNDRV_PCM_INFO_BLOCK_TRANSFER,
    .formats = SNDRV_PCM_FMTBIT_S16_LE,
    .rates = SNDRV_PCM_RATE_48000,
    .rate_min = 48000,
    .rate_max = 48000,
    .channels_min = 2,
    .channels_max = 2,
    .buffer_bytes_max = DMA_BUFFER_SIZE,
    .period_bytes_min = DMA_PERIOD_SIZE,
    .period_bytes_max = DMA_PERIOD_SIZE,
    .periods_min = DMA_PERIODS,
    .periods_max = DMA_PERIODS,
    .fifo_size = 0,
};

// 寄存器定义示例
#define I2S_FORMAT_REG          0x00
#define I2S_CLKDIV_REG         0x04  
#define I2S_CTRL_REG           0x08
#define I2S_TXFIFO_THRESHOLD_REG 0x0C
#define I2S_TX_FIFO_REG        0x10

// I2S控制位
#define I2S_FMT_I2S            (0 << 0)
#define I2S_WDTH_16            (0 << 2)  
#define I2S_MODE_STEREO        (0 << 4)
#define I2S_TXDMA_EN           (1 << 8)
#define I2S_ENABLE             (1 << 0)

// GPIO定义
#define AMP_ENABLE_GPIO        25 