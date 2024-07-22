#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
  namespace hisense_acu2d {

  using remote_base::RemoteTransmitterBase;

  enum Mode : uint8_t { MODE_ONLY_FAN = 0, MODE_HEAT = 1, MODE_COOL = 2, MODE_DRY = 3, MODE_SMART = 4 };
  enum FanSpeed : uint8_t { FAN_MAX = 0, FAN_MIDDLE = 1, FAN_MIN = 2, FAN_AUTO = 3 };
  enum SwingMode : uint8_t { SWING_OFF = 0, SWING_VERTICAL = 1, SWING_HORIZONTAL = 2, SWING_BOTH = 3 };

  class HisenseACU2D : public climate::Climate, public uart::UARTDevice, public Component {
  public:
    void loop() override;
    void dump_config() override;
    void control(const climate::ClimateCall &call) override;
    void set_transmitter(RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }
    void set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) {
      this->supported_swing_modes_ = modes;
    }

  protected:
    climate::ClimateTraits traits() override;
    //void read_state_(const uint8_t *data, uint8_t size);
    void send_data_(const uint8_t *message, uint8_t size);
    //void dump_message_(const char *title, const uint8_t *message, uint8_t size);
    uint8_t get_checksum_(const uint8_t *message, size_t size);
    RemoteTransmitterBase *transmitter_{nullptr};

  private:
    uint8_t data_[25];
    std::set<climate::ClimateSwingMode> supported_swing_modes_{};
  };

  }  // namespace hisense_acu2d
}  // namespace esphome