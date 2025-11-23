#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace hisense_acu2d {

class HisenseACU2DSwitch : public switch_::Switch, public Component {
 protected:
  void write_state(bool state) override { this->publish_state(state); }
};

}  // namespace hisense_acu2d
}  // namespace esphome