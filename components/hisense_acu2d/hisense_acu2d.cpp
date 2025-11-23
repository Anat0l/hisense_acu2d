#include <cmath>
#include "hisense_acu2d.h"
#include "esphome/core/macros.h"

namespace esphome {
namespace hisense_acu2d {

static const char *const TAG = "hisense_acu2d";

static const uint8_t ACU2D_PACKET_LENGTH = 25;
static const uint8_t ACU2D_TIMEOUT_DISPLAY = 100;

static const uint8_t MIN_VALID_TEMPERATURE = 16;
static const uint8_t MAX_VALID_TEMPERATURE = 30;

static const uint16_t WHIRLPOOL_HEADER_MARK = 9000;
static const uint16_t WHIRLPOOL_HEADER_SPACE = 4494;
static const uint16_t WHIRLPOOL_BIT_MARK = 572;
static const uint16_t WHIRLPOOL_ONE_SPACE = 1659;
static const uint16_t WHIRLPOOL_ZERO_SPACE = 553;
static const uint32_t WHIRLPOOL_GAP = 7960;

static const uint8_t WHIRLPOOL_STATE_LENGTH = 21;

static const uint8_t WHIRLPOOL_HEAT = 0;
static const uint8_t WHIRLPOOL_DRY = 3;
static const uint8_t WHIRLPOOL_COOL = 2;
static const uint8_t WHIRLPOOL_FAN = 4;
static const uint8_t WHIRLPOOL_AUTO = 1;

static const uint8_t WHIRLPOOL_FAN_AUTO = 0;
static const uint8_t WHIRLPOOL_FAN_HIGH = 1;
static const uint8_t WHIRLPOOL_FAN_MED = 2;
static const uint8_t WHIRLPOOL_FAN_LOW = 3;

static const uint8_t WHIRLPOOL_SWING_MASK = 128;

static const uint8_t WHIRLPOOL_POWER = 0x04;

static const float TEMPERATURE_STEP = 1.0f;

uint8_t inputBuffer[ACU2D_PACKET_LENGTH] = {0};
uint8_t inputBufferCount = 0;

unsigned int getOnOff(unsigned char *code)
{
  return (code[1] & 0x80)>> 7;
}

unsigned int getWorkMode(unsigned char *code)
{
  return (code[4] & 0x0C) >> 2 ;
}

int getFanSpeed(unsigned char *code)
{

  if ((code[4] & 0x03) == 0X02)
    return 2;
  else if ((code[4] & 0x03) == 0X03)
    return 1;
  else if ((code[4] & 0x03) == 0X01)
    return 0;

  return -1;
}

int getFanMode(unsigned char *code)
{
  if (code[6] == 0X00) // Manual / ручной выбор скорости
    return 0;
  else if (code[6] == 0X10) // Auto / Автоматический выбор скорости
    return 1;
  
  return -1; // Ошибка
}

unsigned int getFan(unsigned char *code)
{
  if (getFanMode(code) == 0)
    return getFanSpeed(code);
  else
    return 3;
}

unsigned char acu2d_crc(unsigned char *pcBlock, unsigned int len = ACU2D_PACKET_LENGTH - 1)
{
    unsigned char crc = 0x00;
    while (len--)
    {
      crc += *pcBlock++;
    }
    return 0xFF - crc;
}

unsigned int getSwigV(unsigned char *code)
{
  return (code[3] & 0x08) >> 3;
}

unsigned int getSwigH(unsigned char *code)
{
  return (code[1] & 0x20) >> 5;
}

unsigned int getSwig(unsigned char *code)
{
  return getSwigV(code) + (getSwigH(code) * 2);
}

unsigned int getDisplay(unsigned char *code)
{
  if ((code[3] & 0x21) == 0x20)
    return 1; // on
  else if ((code[3] & 0x21) == 0x01)
  {
    return 0; // off
  }
  else if ((code[3] & 0x21) == 0x00)
  {
    return 0; // off in sleep mode
  }else
  {
    return 2;
  }
  
}

void HisenseACU2D::dump_config() {
  ESP_LOGCONFIG(TAG, "HiSense AC Uart to Display:");
  this->dump_traits_(TAG);
  this->check_uart_settings(1200);
}

void HisenseACU2D::setup() {
    if (this->sensor_) {
      this->sensor_->add_on_state_callback([this](float state) {
        this->current_temperature = state;
        this->publish_state();
      });
      this->current_temperature = this->sensor_->state;
    }
}

void HisenseACU2D::loop() {

  char rxByte = 0;
  static unsigned long lastRead = 0;
  unsigned int OnOff = 0;
  int SetTemp = 0;  // Заданная температура от +16 до +30
  int CurTemp = 0;  // Температура воздуха с датчика температуры
  bool data_update_ = false;
  
  if ((millis() - lastRead) >= ACU2D_TIMEOUT_DISPLAY)
  {
    lastRead = millis();
    inputBufferCount = 0;
  }
  
  if (available() > 0)
  {
    rxByte = read();
    inputBuffer[inputBufferCount] = rxByte;
    inputBufferCount++;
    lastRead = millis();
  }
  
  if (inputBufferCount >= ACU2D_PACKET_LENGTH)
  {
    if (acu2d_crc(inputBuffer) != inputBuffer[ACU2D_PACKET_LENGTH - 1])
    {
      ESP_LOGCONFIG(TAG, "BAD CRC !!!\r\n");
      return;
    }

    for (unsigned char i {0} ; i< ACU2D_PACKET_LENGTH; i++)
      if (data_[i] != inputBuffer[i])
      {
        data_[i] = inputBuffer[i];
        data_update_ = true;
      }

    // новых данных нет
    if (!data_update_)
      return; 

    ESP_LOGCONFIG(TAG, "-<< %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
      inputBuffer[0],  inputBuffer[1],  inputBuffer[2],  inputBuffer[3],  inputBuffer[4],
      inputBuffer[5],  inputBuffer[6],  inputBuffer[7],  inputBuffer[8],  inputBuffer[9],
      inputBuffer[10], inputBuffer[11], inputBuffer[12], inputBuffer[13], inputBuffer[14],
      inputBuffer[15], inputBuffer[16], inputBuffer[17], inputBuffer[18], inputBuffer[19],
      inputBuffer[20], inputBuffer[21], inputBuffer[22], inputBuffer[23], inputBuffer[24]);
    
    OnOff = getOnOff(inputBuffer);
    
    SetTemp = MAX_VALID_TEMPERATURE - (0x64 - inputBuffer[7])/2;
    CurTemp = MAX_VALID_TEMPERATURE - (0x64 - inputBuffer[8])/2;
    
    if (this->sensor_) {
      CurTemp = this->sensor_->state;
    }

    this->current_temperature = (float)CurTemp;
    this->target_temperature = (float)SetTemp;

    // Режим работы кондиционера
    switch (getWorkMode(inputBuffer)) {
    //case MODE_SMART:
    //  this->mode = climate::CLIMATE_MODE_HEAT_COOL;
    //  break;
    case MODE_COOL:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case MODE_HEAT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      break;
    case MODE_ONLY_FAN:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    default:  // other modes are unsupported
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
    }

    // Режим работы вентилятора
    switch (getFan(inputBuffer)) {
      case FAN_AUTO:
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        break;
      case FAN_MIN:
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        break;
      case FAN_MIDDLE:
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;
      case FAN_MAX:
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        break;
    }

    // Режим работы шторки
    switch (getSwig(inputBuffer)) {
      case SWING_OFF:
        this->swing_mode = climate::CLIMATE_SWING_OFF;
        break;
      case SWING_VERTICAL:
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        break;
      case SWING_HORIZONTAL:
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        break;
      case SWING_BOTH:
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
        break;
    }

    // Статус работы кондиционера
    if (OnOff == 0) {
      this->mode = climate::CLIMATE_MODE_OFF;
    }

    //ESP_LOGCONFIG(TAG, "On/Off:      %u", OnOff);
    //ESP_LOGCONFIG(TAG, "T Set:       %u", SetTemp);
    //ESP_LOGCONFIG(TAG, "T Cur:       %u", CurTemp);
    //ESP_LOGCONFIG(TAG, "WMode:       %u", getWorkMode(inputBuffer));
    //ESP_LOGCONFIG(TAG, "FSpee:       %u", getFanSpeed(inputBuffer));
    //ESP_LOGCONFIG(TAG, "FMode:       %u", getFanMode(inputBuffer));
    //ESP_LOGCONFIG(TAG, "Fan  :       %u", getFan(inputBuffer));
    //ESP_LOGCONFIG(TAG, "Disp :       %u", getDisplay(inputBuffer));
    
    //ESP_LOGCONFIG(TAG, "\r\n");
    
    inputBufferCount = 0;

    // Опубликовать обновленные данные
    this->publish_state();

  }

}

// Статические / базовые / стандартные
// характеристики климатического усройства
climate::ClimateTraits HisenseACU2D::traits() {

  auto traits = climate::ClimateTraits();

//  traits.set_supports_current_temperature(true);
  traits.set_visual_min_temperature(MIN_VALID_TEMPERATURE);
  traits.set_visual_max_temperature(MAX_VALID_TEMPERATURE);
  traits.set_visual_temperature_step(TEMPERATURE_STEP);

  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF, 
    climate::CLIMATE_MODE_HEAT_COOL,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_DRY
  });

  traits.set_supported_fan_modes({
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
  });

  traits.set_supported_swing_modes(this->supported_swing_modes_);

  //traits.set_supports_two_point_target_temperature(false);
  traits.clear_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
  
  //traits.set_supports_current_temperature(true);
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  

//  traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
//  traits.add_supported_preset(climate::CLIMATE_PRESET_COMFORT);

  return traits;
}

void HisenseACU2D::control(const climate::ClimateCall &call) {

  uint8_t remote_state[WHIRLPOOL_STATE_LENGTH] = {0};

/*
  if (call.get_preset().has_value()) {
    if (call.get_preset().value() == climate::CLIMATE_PRESET_COMFORT) {
      data_[POWER] |= COMFORT_PRESET_MASK;
    } else {
      data_[POWER] &= ~COMFORT_PRESET_MASK;
    }
  }
*/
  remote_state[0] = 0x83;
  remote_state[1] = 0x06;
  remote_state[6] = 0x80;
  // MODEL DG11J191
  remote_state[18] = 0x08;

  // Work mode on/off only
  if (call.get_mode().has_value()) {
    ESP_LOGCONFIG(TAG, "HisenseACU2D::control::change mode");
    if ((call.get_mode().value() == climate::CLIMATE_MODE_OFF)  && // 
       (this->mode != climate::CLIMATE_MODE_OFF)
       )
      {
        ESP_LOGCONFIG(TAG, "HisenseACU2D::control::POWER ON");
        remote_state[2] = 4;
        remote_state[15] = 1; 
      }
    if ((call.get_mode().value() != climate::CLIMATE_MODE_OFF) && // 
       (this->mode == climate::CLIMATE_MODE_OFF)
       )
      {
        ESP_LOGCONFIG(TAG, "HisenseACU2D::control::POWER OFF");
        remote_state[2] = 4;
        remote_state[15] = 1; 
      }
  }

  // Work mode
  switch (call.get_mode().has_value() ? call.get_mode().value() : this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      // set fan auto
      // set temp auto temp
      // set sleep false
      remote_state[3] = WHIRLPOOL_AUTO;
      remote_state[15] = 0x17;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[3] = WHIRLPOOL_HEAT;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[3] = WHIRLPOOL_COOL;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[3] = WHIRLPOOL_DRY;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[3] = WHIRLPOOL_FAN;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      break;
  }

  // Temperature
  auto temp = (uint8_t) roundf(call.get_target_temperature().has_value() ? call.get_target_temperature().value() : this->target_temperature);
  remote_state[3] |= (uint8_t) (temp - MIN_VALID_TEMPERATURE) << 4;

  // Fan speed
  switch (call.get_fan_mode().has_value() ? call.get_fan_mode().value() : this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state[2] |= WHIRLPOOL_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[2] |= WHIRLPOOL_FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[2] |= WHIRLPOOL_FAN_LOW;
      break;
    default:
      break;
  }

  // Swing
  if (call.get_swing_mode().has_value()) {
    switch (call.get_swing_mode().value()) {
      case climate::CLIMATE_SWING_OFF:
        if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL || this->swing_mode == climate::CLIMATE_SWING_BOTH)
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_VERTICAL::OFF");
          remote_state[2] |= 0x80;
          remote_state[8] |= 0x40;
        }
        if (this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL || this->swing_mode == climate::CLIMATE_SWING_BOTH)
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_HORIZONTAL::OFF");
          remote_state[8] |= 0x80;
          remote_state[15] = 0x08;
        }
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        if (this->swing_mode == climate::CLIMATE_SWING_BOTH)
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_HORIZONTAL::OFF");
          remote_state[8] |= 0x80;
          remote_state[15] = 0x08;
        } else if (this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL)
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_HORIZONTAL::OFF");
          remote_state[8] |= 0x80;
          remote_state[15] = 0x08;
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_VERTICAL::ON");
          remote_state[2] |= 0x80;
          remote_state[8] |= 0x40;
        } else
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_VERTICAL::ON");
          remote_state[2] |= 0x80;
          remote_state[8] |= 0x40;
        }
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        if (this->swing_mode == climate::CLIMATE_SWING_BOTH ) // 
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_VERTICAL::OFF");
          remote_state[2] |= 0x80;
          remote_state[8] |= 0x40;
        } else if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL)
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_VERTICAL::OFF");
          remote_state[2] |= 0x80;
          remote_state[8] |= 0x40;
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_HORIZONTAL::ON");
          remote_state[8] |= 0x80;
          remote_state[15] = 0x08;
        } else                 
        {
        ESP_LOGCONFIG(TAG, "CLIMATE_SWING_HORIZONTAL::ON");
        remote_state[8] |= 0x80;
        remote_state[15] = 0x08;
        }
        break;
      case climate::CLIMATE_SWING_BOTH:
        if (this->swing_mode != climate::CLIMATE_SWING_VERTICAL)
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_VERTICAL::ON");
          remote_state[2] |= 0x80;
          remote_state[8] |= 0x40;
        }
        if (this->swing_mode != climate::CLIMATE_SWING_HORIZONTAL)
        {
          ESP_LOGCONFIG(TAG, "CLIMATE_SWING_HORIZONTAL::ON");
          remote_state[8] |= 0x80;
          remote_state[15] = 0x08;
        }
        break;
    }
  }

  // Checksum
  for (uint8_t i = 2; i < 13; i++)
    remote_state[13] ^= remote_state[i];
  for (uint8_t i = 14; i < 20; i++)
    remote_state[20] ^= remote_state[i];

  // Send ir code
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(WHIRLPOOL_HEADER_MARK);
  data->space(WHIRLPOOL_HEADER_SPACE);
  // Data
  auto bytes_sent = 0;
  for (uint8_t i : remote_state) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(WHIRLPOOL_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? WHIRLPOOL_ONE_SPACE : WHIRLPOOL_ZERO_SPACE);
    }
    bytes_sent++;
    if (bytes_sent == 6 || bytes_sent == 14) {
      // Divider
      data->mark(WHIRLPOOL_BIT_MARK);
      data->space(WHIRLPOOL_GAP);
    }
  }
  // Footer
  data->mark(WHIRLPOOL_BIT_MARK);

  transmit.perform();
}

void HisenseACU2D::send_data_(const uint8_t *message, uint8_t size) {
  this->write_array(message, size);

  //dump_message_("Sent message", message, size);
}

//void HisenseACU2D::dump_message_(const char *title, const uint8_t *message, uint8_t size) {
//  ESP_LOGV(TAG, "%s:", title);
//  for (int i = 0; i < size; i++) {
//    ESP_LOGV(TAG, "  byte %02d - %d", i, message[i]);
//  }
//}

uint8_t HisenseACU2D::get_checksum_(const uint8_t *message, size_t size) {
  uint8_t position = size - 1;
  uint8_t crc = 0;

  for (int i = 2; i < position; i++)
    crc += message[i];

  return crc;
}

void HisenseACU2D::set_ifeel_switch(switch_::Switch *ifeel_switch) {

  this->ifeel_switch_ = ifeel_switch;
  this->ifeel_switch_->add_on_state_callback([this](bool state) {
    ESP_LOGD(TAG, "debug set_ifeel_switch. ");
    if (state == this->ifeel_state_)
      return;
    ESP_LOGD(TAG, "set_ifeel_switch. ");
//    this->on_ifeel_change(state);
  });
}

}  // namespace hisense_acu2d
}  // namespace esphome
