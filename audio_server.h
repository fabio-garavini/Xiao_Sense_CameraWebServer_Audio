#include <driver/i2s.h>
#include <WebServer.h>

#include "camera_pins.h"

WebServer AudioServer(82);
WebServer VideoAudioServer(83);

const int sampleRate = SAMPLE_RATE; // Sample rate of the audio
const int bitsPerSample = SAMPLE_BITS; // Bits per sample of the audio
const int numChannels = 1; // Number of audio channels (1 for mono, 2 for stereo)
const int bufferSize = DMA_BUF_LEN; // Buffer size for I2S data transfer

struct WAVHeader {
  char chunkId[4];          // 4 bytes
  uint32_t chunkSize;       // 4 bytes
  char format[4];           // 4 bytes
  char subchunk1Id[4];      // 4 bytes
  uint32_t subchunk1Size;   // 4 bytes
  uint16_t audioFormat;     // 2 bytes
  uint16_t numChannels;     // 2 bytes
  uint32_t sampleRate;      // 4 bytes
  uint32_t byteRate;        // 4 bytes
  uint16_t blockAlign;      // 2 bytes
  uint16_t bitsPerSample;   // 2 bytes
  char subchunk2Id[4];      // 4 bytes
  uint32_t subchunk2Size;   // 4 bytes
};

void initializeWAVHeader(WAVHeader &header, uint32_t sampleRate, uint16_t bitsPerSample, uint16_t numChannels) {

  strncpy(header.chunkId, "RIFF", 4);
  strncpy(header.format, "WAVE", 4);
  strncpy(header.subchunk1Id, "fmt ", 4);
  strncpy(header.subchunk2Id, "data", 4);

  header.chunkSize = 0; // Placeholder for Chunk Size (to be updated later)
  header.subchunk1Size = 16; // PCM format size (constant for uncompressed audio)
  header.audioFormat = 1; // PCM audio format (constant for uncompressed audio)
  header.numChannels = numChannels;
  header.sampleRate = sampleRate;
  header.bitsPerSample = bitsPerSample;
  header.byteRate = (sampleRate * bitsPerSample * numChannels) / 8;
  header.blockAlign = (bitsPerSample * numChannels) / 8;
  header.subchunk2Size = 0; // Placeholder for data size (to be updated later)
}

void mic_i2s_init() {

  i2s_config_t i2sConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = SAMPLE_RATE ,
    .bits_per_sample = i2s_bits_per_sample_t(SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = true
  };
  i2s_driver_install(I2S_PORT, &i2sConfig, 0, NULL);

  i2s_pin_config_t pinConfig = {
    .bck_io_num = I2S_SCK, 
    .ws_io_num = I2S_WS ,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD 
  };
  i2s_set_pin(I2S_PORT, &pinConfig);
}

void handleAudioStream() {

  mic_i2s_init();

  WAVHeader wavHeader;
  initializeWAVHeader(wavHeader, sampleRate, bitsPerSample, numChannels);

  // Get access to the client object
  WiFiClient Audioclient = AudioServer.client();

  // Send the 200 OK response with the headers
  Audioclient.print("HTTP/1.1 200 OK\r\n");
  Audioclient.print("Content-Type: audio/wav\r\n");
  Audioclient.print("Access-Control-Allow-Origin: *\r\n");
  Audioclient.print("\r\n");

  // Send the initial part of the WAV header
  Audioclient.write(reinterpret_cast<const uint8_t*>(&wavHeader), sizeof(wavHeader));

  uint8_t buffer[bufferSize];
  size_t bytesRead = 0;
  //uint32_t totalDataSize = 0; // Total size of audio data sent

  while (true) {
    if (!Audioclient.connected()) {
      //i2s_driver_uninstall(I2S_PORT);
      Serial.println("Audioclient disconnected");
      break;
    }
    // Read audio data from I2S DMA
    i2s_read(I2S_PORT, buffer, bufferSize, &bytesRead, portMAX_DELAY);

    // Send audio data
    if (bytesRead > 0) {
      Audioclient.write(buffer, bytesRead);
      //totalDataSize += bytesRead;
      //Serial.println(totalDataSize);
    }
  }
}

void startAudioServer() {

  AudioServer.on("/audio", HTTP_GET, handleAudioStream);
  AudioServer.begin();

}

void handleVideAudio(){

  String ip = WiFi.localIP().toString();

  String content = "<!DOCTYPE html><html lang=\"en\">";
  content += "<head><meta charset=\"UTF-8\">";
  content += "<title>ESP32-CAM + Livestream Audio</title>";
  content += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  content += "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">";
  content += "<style>";
  content += "body { background-color: #FFF; }";
  content += ".container1 { display: flex; flex-direction: row; justify-content: center; align-items: center; height: 100%; }";
  content += ".container2 { display: flex; flex-direction: row; justify-content: center; align-items: center; height: 100%; }";
  content += ".block { width: 200px; height: 200px; background-color: gray; margin: 10px; display: flex; justify-content: center; align-items: center; }";
  content += "#video { border: 0; }";
  content += "#audio { border: 0; }";
  content += "#tbl { background-color: #000; }";
  content += "</style></head><body>";
  content += "<h1>ESP32-CAM + Livestream Audio</h1>";
  content += "<div class=\"container1\" style=\"overflow-x:auto;\">";
  content += "<table id=\"tbl\" style=\"overflow-x:auto;\"><tr>";
  content += "<td><iframe id=\"video\" src=\"http://" + ip + ":81/stream\" allow=\"camera\" width=\"640\" height=\"480\"></iframe></td>";
  content += "</tr><tr>";
  content += "<td><video controls autoplay width=\"640\" height=\"60\" id=\"audio\"><source src=\"http://" + ip + ":82/audio\" type=\"audio/wav\"></video></td>";
  content += "</tr></table></div>";
  content += "</body></html>";

  VideoAudioServer.send(200, "text/html", content);

}

void startVideoAudioServer(){

  VideoAudioServer.on("/", HTTP_GET, handleVideAudio);
  VideoAudioServer.begin();

}
