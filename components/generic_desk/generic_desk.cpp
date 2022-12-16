#include "esphome/core/log.h"
#include "generic_desk.h"

//last_signal is just the last time input was read from buttons or from controller
//If we haven't seen anything from either in a bit, stop moving
uint32_t last_signal = 0;
uint32_t signal_giveup_time = 2000;

LogicData ld(-1);

//-- Buffered mode parses input words and sends them to output separately
void ICACHE_RAM_ATTR logicDataPin_ISR() {
  ld.PinChange(HIGH == digitalRead(LOGICDATA_RX));
}

uint8_t highTarget = 41;
uint8_t lowTarget = 28;


uint8_t target;

namespace esphome
{

    namespace generic_desk
    {
        // Record last time the display changed
        // sets globals height and last_signal
        void GenericDesk::check_display() {
          uint8_t height;
          static uint32_t prev = 0;
          uint32_t msg = ld.ReadTrace();
//          ESP_LOGD("MSG", "%d", msg);
          char buf[80];
          if (msg) {
            uint32_t now = millis();
//            sprintf(buf, "%6ums %s: %s", now - prev, ld.MsgType(msg), ld.Decode(msg));
//            ESP_LOGD("BUF", "%6ums %s: %s", now - prev, ld.MsgType(msg), ld.Decode(msg));
//            Serial.println(buf);
            prev=now;
          }

          // Reset idle-activity timer if display number changes or if any other display activity occurs (i.e. display-ON)
          if (ld.IsNumber(msg)) {
            auto new_height = ld.GetNumber(msg);
            if (new_height == height) {
              return;
            }
            this->height = float(new_height);
          }
          if (msg)
            last_signal = millis();
        }

        static const char *TAG = "generic_desk.sensor";

        void GenericDesk::setup()
        {
            pinMode(LOGICDATA_RX, INPUT);
//            pinMode(LOGICDATA_RX, INPUT_PULLDOWN_16);

            logicDataPin_ISR();
            attachInterrupt(digitalPinToInterrupt(LOGICDATA_RX), logicDataPin_ISR, CHANGE);

            ld.Begin();
        }

        void GenericDesk::loop()
        {
            check_display();
//            ESP_LOGD("Height", "%d", height);
        }

        void GenericDesk::update()
        {
            // Publish the desk height if it changed
            if (height != last_height)
            {
                last_height = height;
                for (auto *height_sensor : this->height_sensors)
                    height_sensor->publish_state(height);
                for (auto *moving_sensor : this->moving_sensors)
                    moving_sensor->publish_state(true);
                desk_moving_debounce_counter = 0;
            }

            // Debounce desk is moving to prevent state flickering
            if (desk_moving_debounce_counter == DESK_MOVING_DEBOUNCE_THRESHOLD)
            {
                // Set all sensors to false
                for (auto *moving_sensor : this->moving_sensors)
                    moving_sensor->publish_state(false);

                // Reset switches
                for (auto *desk_switch : this->desk_switches)
                    desk_switch->publish_state(false);

                desk_moving_debounce_counter++;
            }
            if (desk_moving_debounce_counter < DESK_MOVING_DEBOUNCE_THRESHOLD)
            {
                desk_moving_debounce_counter++;
            }
        }

        void GenericDesk::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Generic SitStand Desk");
            for (auto *height_sensor : this->height_sensors)
                LOG_SENSOR("", "Height sensor: ", height_sensor);
            for (auto *moving_sensor : this->moving_sensors)
                LOG_BINARY_SENSOR("", "Is Moving binary sensor: ", moving_sensor);
        }

        /**
         * @brief CRC-16
         *
         * @param data The Data to calculate the Checksum on
         * @param len The Nr of Bytes in Data
         * @return Checksum for data
         */
        uint16_t GenericDesk::crc16(const uint8_t *data, uint8_t len)
        {
            uint16_t crc = 0xFFFF;
            while (len--)
            {
                crc ^= *data++;
                for (uint8_t i = 0; i < 8; i++)
                {
                    if ((crc & 0x01) != 0)
                    {
                        crc >>= 1;
                        crc ^= 0xA001;
                    }
                    else
                    {
                        crc >>= 1;
                    }
                }
            }
            return crc >> 8 | crc << 8;
        }

    } // namespace generic_desk
} // namespace esphome