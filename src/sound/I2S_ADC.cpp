#include "sound/I2S_ADC.h"

void setupADC(adc1_channel_t adcPin) {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(adcPin, ADC_ATTEN_DB_12);
}

void setAdcPin(esp_err_t & ret, adc1_channel_t pin) {
  ret = i2s_set_adc_mode(ADC_UNIT_1, pin);
  if (ret != ESP_OK) {
    Serial.printf("ОШИБКА настройки режима ADC: %s\n", esp_err_to_name(ret));
    while (1);
  }
}

esp_err_t setupI2S(adc1_channel_t adcPin, uint32_t rate) {
  i2s_config_t i2s_config = {
      .mode =
          (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
      .sample_rate = rate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 1024,
      .use_apll = true,  // APLL может дать более точные частоты
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  esp_err_t ret = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (ret != ESP_OK) {
    Serial.printf("ОШИБКА установки I2S: %s\n", esp_err_to_name(ret));
    while (1);
  }

  setAdcPin(ret, adcPin);

  clearI2SBuffers();  // Очищаем буферы перед началом

  esp_err_t ret2 = i2s_adc_enable(I2S_NUM_0);
  if (ret2 != ESP_OK) {
    Serial.printf("ОШИБКА включения ADC: %s\n", esp_err_to_name(ret2));
    while (1);
  }

  return ret;
}

void clearI2SBuffers() {
  // Очищаем внутренние буферы I2S
  size_t dummy_bytes = 0;
  uint8_t dummy_buffer[512];

  Serial.println("Очистка буферов I2S...");

  // Читаем и выбрасываем данные в течение 200 мс
  unsigned long clear_start = millis();
  while (millis() - clear_start < 200) {
    i2s_read(I2S_NUM_0, dummy_buffer, sizeof(dummy_buffer), &dummy_bytes,
             pdMS_TO_TICKS(10));
  }

  Serial.println("Буферы очищены");
}
