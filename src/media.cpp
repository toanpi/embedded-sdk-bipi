#include <opus.h>
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include "esp_log.h"
#include "main.h"
#include "freertos/FreeRTOS.h"

#define OPUS_OUT_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

static const char *TAG = "media";
static i2s_chan_handle_t rx_chan;        // I2S rx channel handler
static i2s_chan_handle_t tx_chan;        // I2S tx channel handler

// static void init_microphone_i2s(void)
// {
//     /* Configure I2S channel using settings from main.c */
//     i2s_chan_config_t chan_cfg = {
//         .id = I2S_NUM_1,
//         .role = I2S_ROLE_MASTER,
//         .dma_desc_num = 8,      // Increased from 4 to 8
//         .dma_frame_num = 2048,    // Increased from 1024 to 2048
//         .auto_clear = false,      // Match main.c (auto_clear = false)
//     };
    
//     esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &rx_chan);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
//         return;
//     }

//     /* Configure I2S standard mode using settings from main.c */
//     i2s_std_config_t std_cfg = {
//         .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
//         .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
//         .gpio_cfg = {
//             .mclk = I2S_GPIO_UNUSED,
//             .bclk = GPIO_NUM_2,
//             .ws = GPIO_NUM_3,
//             .dout = I2S_GPIO_UNUSED,
//             .din = GPIO_NUM_5,
//             .invert_flags = {
//                 .mclk_inv = false,
//                 .bclk_inv = false,
//                 .ws_inv = false,
//             },
//         },
//     };
    
//     std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    
//     ESP_LOGI(TAG, "Initializing I2S channel in standard mode...");
//     ESP_LOGI(TAG, "Sample rate: %d Hz, Bit width: 16-bit, Mode: Mono, Format: Philips");
//     ESP_LOGI(TAG, "GPIO config - BCLK: %d, WS: %d, DIN: %d", 
//              std_cfg.gpio_cfg.bclk, std_cfg.gpio_cfg.ws, std_cfg.gpio_cfg.din);
    
//     ret = i2s_channel_init_std_mode(rx_chan, &std_cfg);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to initialize I2S channel in standard mode: %s", esp_err_to_name(ret));
//         return;
//     }

//     /* Enable the RX channel */
//     ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    
//     ESP_LOGI(TAG, "I2S microphone initialized successfully");
// }

static void init_speaker_i2s(void)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = BUFFER_SAMPLES,
        .auto_clear = true,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return;
    } 

    /* Configure I2S standard mode using settings from main.c */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = { 
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_9,
            .ws = GPIO_NUM_10,
            .dout = GPIO_NUM_8,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    // Enable APLL for better audio quality if available
    // Check header files for correct values before enabling
#if defined(I2S_CLK_SRC_APLL)
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
#endif
    
    ret = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S channel in standard mode: %s", esp_err_to_name(ret)); 
        return;
    }

    /* Enable the TX channel */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan)); 

    ESP_LOGI(TAG, "I2S speaker initialized successfully");
}


void lk_init_audio_capture() {
  // init_microphone_i2s();
  init_speaker_i2s();
}

opus_int16 *output_buffer = NULL;
OpusDecoder *opus_decoder = NULL;

void lk_init_audio_decoder() {
  int decoder_error = 0;
  opus_decoder = opus_decoder_create(SAMPLE_RATE, 2, &decoder_error);
  if (decoder_error != OPUS_OK) {
    printf("Failed to create OPUS decoder");
    return;
  }

  // Allocate a stereo buffer (2 channels)
  output_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES * 2 * sizeof(opus_int16));
  if (output_buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate stereo output buffer");
    return;
  }
}

void lk_audio_decode(uint8_t *data, size_t size) {
  int decoded_size =
      opus_decode(opus_decoder, data, size, output_buffer, BUFFER_SAMPLES, 0);

  if (decoded_size > 0) {
    size_t bytes_written = 0;
    // Calculate the actual size to write based on the decoded samples
    // For stereo, each sample consists of 2 channels × 2 bytes per sample (16-bit)
    size_t write_size = decoded_size * 2 * sizeof(int16_t);
    
    ESP_LOGD(TAG, "Decoded %d samples, writing %d bytes", decoded_size, write_size);
    
    esp_err_t ret = i2s_channel_write(tx_chan, output_buffer, write_size, 
                                      &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write audio data: %s", esp_err_to_name(ret));
    }
  }
}

OpusEncoder *opus_encoder = NULL;
opus_int16 *encoder_input_buffer = NULL;
uint8_t *encoder_output_buffer = NULL;

void lk_init_audio_encoder() {
  int encoder_error;
  opus_encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP,
                                     &encoder_error);
  if (encoder_error != OPUS_OK) {
    printf("Failed to create OPUS encoder");
    return;
  }

  if (opus_encoder_init(opus_encoder, SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP) !=
      OPUS_OK) {
    printf("Failed to initialize OPUS encoder");
    return;
  }

  opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
  opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
  opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  encoder_input_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES);
  encoder_output_buffer = (uint8_t *)malloc(OPUS_OUT_BUFFER_SIZE);
}

void lk_send_audio(PeerConnection *peer_connection) {
  size_t bytes_read = 0;

  // i2s_channel_read(rx_chan, encoder_input_buffer, BUFFER_SAMPLES, &bytes_read,
  //          portMAX_DELAY);

  auto encoded_size =
      opus_encode(opus_encoder, encoder_input_buffer, BUFFER_SAMPLES / 2,
                  encoder_output_buffer, OPUS_OUT_BUFFER_SIZE);

  peer_connection_send_audio(peer_connection, encoder_output_buffer,
                             encoded_size);
}
