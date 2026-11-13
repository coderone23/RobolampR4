#include "esphome.h"
#include "driver/i2c.h"

#define I2C_SLAVE_NUM      I2C_NUM_0
#define I2C_SLAVE_ADDR     0x42
#define I2C_SDA_PIN        23
#define I2C_SCL_PIN        24

class I2CServoSlave : public Component {
 private:
  uint8_t motor_0_last_status = 0;
  uint8_t motor_1_last_status = 0;

 public:
  void setup() override {
    ESP_LOGI("i2c_servo", "Starting I2C Setup Component...");

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_SLAVE;
    conf.sda_io_num = (gpio_num_t)I2C_SDA_PIN;
    conf.scl_io_num = (gpio_num_t)I2C_SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.slave.addr_10bit_en = 0;
    conf.slave.slave_addr = I2C_SLAVE_ADDR;

    esp_err_t err1 = i2c_param_config(I2C_SLAVE_NUM, &conf);
    esp_err_t err2 = i2c_driver_install(I2C_SLAVE_NUM, conf.mode, 256, 256, 0);

    if (err1 != ESP_OK || err2 != ESP_OK) {
      ESP_LOGE("i2c_servo", "FAILED to initialize I2C! Error codes: %d, %d", err1, err2);
    } else {
      ESP_LOGI("i2c_servo", "I2C Slave initialized successfully!");
    }
  }

  void loop() override {
    uint8_t data[6];
    while (true) {
      int size = i2c_slave_read_buffer(I2C_SLAVE_NUM, data, 6, 0);
      if (size <= 0) break;

      uint8_t message_id = data[0];
      uint8_t motor_id = data[1];

      if (size == 6) {
        uint32_t microseconds = (uint32_t)data[2] | ((uint32_t)data[3] << 8) | ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 24);
        float duty_cycle = (float)microseconds / 20000.0f;

        ESP_LOGI("i2c_servo", "Parsed WRITE -> Motor: %d | uS: %lu | Duty: %.3f", motor_id, microseconds, duty_cycle);

        if (motor_id == 0) {
          id(servo_1_pwm).set_level(duty_cycle);
          motor_0_last_status = 1;
        } else if (motor_id == 1) {
          id(servo_2_pwm).set_level(duty_cycle);
          motor_1_last_status = 1;
        }
      }
      else if (size == 2) {
        ESP_LOGI("i2c_servo", "Parsed READ request -> Motor: %d", motor_id);
        uint8_t response_byte = (motor_id == 0) ? motor_0_last_status : motor_1_last_status;
        i2c_slave_write_buffer(I2C_SLAVE_NUM, &response_byte, 1, 100 / portTICK_PERIOD_MS);
      }
    }
  }
};
