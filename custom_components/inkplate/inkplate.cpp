#include "inkplate.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace inkplate {

static const char *TAG = "inkplate";

void Inkplate::setup() {
  this->initialize_();

  this->vcom_pin_->setup();
  this->powerup_pin_->setup();
  this->wakeup_pin_->setup();
  this->gpio0_enable_pin_->setup();
  this->gpio0_enable_pin_->digital_write(true);

  this->cl_pin_->setup();
  this->le_pin_->setup();
  this->ckv_pin_->setup();
  this->gmod_pin_->setup();
  this->oe_pin_->setup();
  this->sph_pin_->setup();
  this->spv_pin_->setup();

  this->display_data_0_pin_->setup();
  this->display_data_1_pin_->setup();
  this->display_data_2_pin_->setup();
  this->display_data_3_pin_->setup();
  this->display_data_4_pin_->setup();
  this->display_data_5_pin_->setup();
  this->display_data_6_pin_->setup();
  this->display_data_7_pin_->setup();

  this->clean();
  this->display();
}
void Inkplate::initialize_() {
  uint32_t buffer_size = this->get_buffer_length_();

  if (this->partial_buffer_ != nullptr) {
    free(this->partial_buffer_);
  }
  if (this->partial_buffer_2_ != nullptr) {
    free(this->partial_buffer_2_);
  }
  if (this->buffer_ != nullptr) {
    free(this->buffer_);
  }

  this->buffer_ = (uint8_t *)ps_malloc(buffer_size);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
  }
  if(!this->greyscale_) {
    this->partial_buffer_ = (uint8_t *)ps_malloc(buffer_size);
    if (this->partial_buffer_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate partial buffer for display!");
    }
    this->partial_buffer_2_ = (uint8_t *)ps_malloc(buffer_size * 2);
    if (this->partial_buffer_2_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate partial buffer 2 for display!");
    }
    memset(this->partial_buffer_, 0, buffer_size);
    memset(this->partial_buffer_2_, 0, buffer_size * 2);
  }

  memset(this->buffer_, 0, buffer_size);
}
float Inkplate::get_setup_priority() const { return setup_priority::PROCESSOR; }
size_t Inkplate::get_buffer_length_() {
  if (this->greyscale_) {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 2u;
  } else {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
  }
}
void Inkplate::update() {
  this->do_update_();

  if (this->full_update_every_ > 0 && this->partial_updates_ >= this->full_update_every_) {
    this->block_partial_ = true;
  }

  this->display();
}
void HOT Inkplate::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  if (this->greyscale_) {
    int x1 = x / 2;
    int x_sub = x % 2;
    uint32_t pos = (x1 + y * (this->get_width_internal() / 2));
    uint8_t current = this->buffer_[pos];

    // float px = (0.2126 * (color.red / 255.0)) + (0.7152 * (color.green / 255.0)) + (0.0722 * (color.blue / 255.0));
    // px = pow(px, 1.5);
    // uint8_t gs = (uint8_t)(px*7);

    uint8_t gs = ((color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000)) >> 5;
    this->buffer_[pos] = (pixelMaskGLUT[x_sub] & current) | (x_sub ? gs : gs << 4);

  } else {
    int x1 = x / 8;
    int x_sub = x % 8;
    uint32_t pos = (x1 + y * (this->get_width_internal() / 8));
    uint8_t current = this->partial_buffer_[pos];
    this->partial_buffer_[pos] = (~pixelMaskLUT[x_sub] & current) | (color.is_on() ? 0 : pixelMaskLUT[x_sub]);
  }
}
void Inkplate::dump_config() {
  LOG_DISPLAY("", "Inkplate", this);
  ESP_LOGCONFIG(TAG, "  Greyscale: %s", YESNO(this->greyscale_));
  ESP_LOGCONFIG(TAG, "  Partial Updating: %s", YESNO(this->partial_updating_));
  ESP_LOGCONFIG(TAG, "  Full Update Every: %d", this->full_update_every_);
  // Log pins
  LOG_PIN("  CKV Pin: ", this->ckv_pin_);
  LOG_PIN("  CL Pin: ", this->cl_pin_);
  LOG_PIN("  GPIO0 Enable Pin: ", this->gpio0_enable_pin_);
  LOG_PIN("  GMOD Pin: ", this->gmod_pin_);
  LOG_PIN("  LE Pin: ", this->le_pin_);
  LOG_PIN("  OE Pin: ", this->oe_pin_);
  LOG_PIN("  POWERUP Pin: ", this->powerup_pin_);
  LOG_PIN("  SPH Pin: ", this->sph_pin_);
  LOG_PIN("  SPV Pin: ", this->spv_pin_);
  LOG_PIN("  VCOM Pin: ", this->vcom_pin_);
  LOG_PIN("  WAKEUP Pin: ", this->wakeup_pin_);

  LOG_PIN("  Data 0 Pin: ", this->display_data_0_pin_);
  LOG_PIN("  Data 1 Pin: ", this->display_data_1_pin_);
  LOG_PIN("  Data 2 Pin: ", this->display_data_2_pin_);
  LOG_PIN("  Data 3 Pin: ", this->display_data_3_pin_);
  LOG_PIN("  Data 4 Pin: ", this->display_data_4_pin_);
  LOG_PIN("  Data 5 Pin: ", this->display_data_5_pin_);
  LOG_PIN("  Data 6 Pin: ", this->display_data_6_pin_);
  LOG_PIN("  Data 7 Pin: ", this->display_data_7_pin_);

  LOG_UPDATE_INTERVAL(this);
}
void Inkplate::eink_off_() {
  ESP_LOGV(TAG, "Eink off called");
  unsigned long start_time = millis();
  if (panel_on_ == 0)
    return;
  panel_on_ = 0;
  this->gmod_pin_->digital_write(false);
  this->oe_pin_->digital_write(false);

  GPIO.out &= ~(get_data_pin_mask_() | (1 << this->cl_pin_->get_pin())
                                    | (1 << this->le_pin_->get_pin()));

  this->sph_pin_->digital_write(false);
  this->spv_pin_->digital_write(false);

  this->powerup_pin_->digital_write(false);
  this->wakeup_pin_->digital_write(false);
  this->vcom_pin_->digital_write(false);

  pins_z_state_();
}
void Inkplate::eink_on_() {
  ESP_LOGV(TAG, "Eink on called");
  unsigned long start_time = millis();
  if (panel_on_ == 1)
    return;
  panel_on_ = 1;
  pins_as_outputs_();
  this->wakeup_pin_->digital_write(true);
  this->powerup_pin_->digital_write(true);
  this->vcom_pin_->digital_write(true);

  this->write_byte(0x01, 0x3F);

  delay(40);

  this->write_byte(0x0D, 0x80);

  delay(2);

  this->read_byte(0x00, &temperature_, 0);

  this->le_pin_->digital_write(false);
  this->oe_pin_->digital_write(false);
  this->cl_pin_->digital_write(false);
  this->sph_pin_->digital_write(true);
  this->gmod_pin_->digital_write(true);
  this->spv_pin_->digital_write(true);
  this->ckv_pin_->digital_write(false);
  this->oe_pin_->digital_write(true);
}
void Inkplate::fill(Color color) {
  ESP_LOGV(TAG, "Fill called");
  unsigned long start_time = millis();

  if(this->greyscale_) {
    uint8_t fill = ((color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000)) >> 5;
    for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
      this->buffer_[i] = (fill << 4) | fill;
  } else {
    uint8_t fill = color.is_on() ? 0x00 : 0xFF;
    for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
      this->partial_buffer_[i] = fill;
  }

  ESP_LOGV(TAG, "Fill finished (%dms)", millis() - start_time);
}
void Inkplate::display() {
  ESP_LOGV(TAG, "Display called");
  unsigned long start_time = millis();

  if (this->greyscale_) {
    this->display3b_();
  } else {
    if (this->partial_updating_ && this->partial_update_()) {
      ESP_LOGV(TAG, "Display finished (partial) (%dms)", millis() - start_time);
      return;
    }
    this->display1b_();
  }
  ESP_LOGV(TAG, "Display finished (full) (%dms)", millis() - start_time);
}
void Inkplate::display1b_() {
  ESP_LOGV(TAG, "Display1b called");
  unsigned long start_time = millis();

  for (int i = 0; i < this->get_buffer_length_(); i++) {
    this->buffer_[i] &= this->partial_buffer_[i];
    this->buffer_[i] |= this->partial_buffer_[i];
  }

  uint16_t pos;
  uint32_t send;
  uint8_t data;
  uint8_t buffer_value;
  eink_on_();
  clean_fast_(0, 1);
  clean_fast_(1, 5);
  clean_fast_(2, 1);
  clean_fast_(0, 5);
  clean_fast_(2, 1);
  clean_fast_(1, 12);
  clean_fast_(2, 1);
  clean_fast_(0, 11);

  ESP_LOGV(TAG, "Display1b start loops (%dms)", millis() - start_time);
  for (int k = 0; k < 3; k++) {
    pos = this->get_buffer_length_() - 1;
    vscan_start_();
    for (int i = 0; i < this->get_height_internal(); i++) {
      buffer_value = this->buffer_[pos];
      data = LUTB[(buffer_value >> 4) & 0x0F];
      send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
      hscan_start_(send);
      data = LUTB[buffer_value & 0x0F];
      send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      pos--;

      for (int j=0; j < (this->get_width_internal() / 8) - 1; j++) {
        buffer_value =this->buffer_[pos];
        data = LUTB[(buffer_value >> 4) & 0x0F];
        send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
        GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
        GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
        data = LUTB[buffer_value & 0x0F];
        send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
        GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
        GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
        pos--;
      }
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_()  | (1 << this->cl_pin_->get_pin());
      vscan_end_();
    }
    delayMicroseconds(230);
  }
  ESP_LOGV(TAG, "Display1b first loop x %d (%dms)", 3, millis() - start_time);

  pos = this->get_buffer_length_() - 1;
  vscan_start_();
  for (int i = 0; i < this->get_height_internal(); i++) {
    buffer_value = this->buffer_[pos];
    data = LUT2[(buffer_value >> 4) & 0x0F];
    send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
    hscan_start_(send);
    data = LUT2[buffer_value & 0x0F];
    send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
    GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
    GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
    pos--;
    for (int j = 0; j < (this->get_width_internal() / 8) - 1; j++) {
      buffer_value =this->buffer_[pos];
      data = LUT2[(buffer_value >> 4) & 0x0F];
      send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      data = LUT2[buffer_value & 0x0F];
      send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      pos--;
    }
    GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
    GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
    vscan_end_();
  }
  delayMicroseconds(230);
  ESP_LOGV(TAG, "Display1b second loop (%dms)", millis() - start_time);

  vscan_start_();
  for (int i = 0; i < this->get_height_internal(); i++) {
    buffer_value = this->buffer_[pos];
    data = 0b00000000;
		send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
    hscan_start_(send);
    GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
    GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
    for (int j = 0; j < (this->get_width_internal() / 8) - 1; j++) {
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
    }
    GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
    GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
    vscan_end_();
  }
  delayMicroseconds(230);
  ESP_LOGV(TAG, "Display1b third loop (%dms)", millis() - start_time);

  vscan_start_();
  eink_off_();
  this->block_partial_ = false;
  this->partial_updates_ = 0;
  ESP_LOGV(TAG, "Display1b finished (%dms)", millis() - start_time);
}
void Inkplate::display3b_() {
  ESP_LOGV(TAG, "Display3b called");
  unsigned long start_time = millis();

  eink_on_();
  clean_fast_(0, 1);
  clean_fast_(1, 12);
  clean_fast_(2, 1);
  clean_fast_(0, 11);
  clean_fast_(2, 1);
  clean_fast_(1, 12);
  clean_fast_(2, 1);
  clean_fast_(0, 11);

  for (int k = 0; k < 8; k++) {
    uint32_t pos = this->get_buffer_length_() - 1;
    uint32_t send;
    uint8_t pix1;
    uint8_t pix2;
    uint8_t pix3;
    uint8_t pix4;
    uint8_t pixel;
    uint8_t pixel2;

    vscan_start_();
    for (int i = 0; i < this->get_height_internal(); i++) {
      pix1 = this->buffer_[pos--];
      pix2 = this->buffer_[pos--];
      pix3 = this->buffer_[pos--];
      pix4 = this->buffer_[pos--];
      pixel = (waveform3Bit[pix1 & 0x07][k] << 6) | (waveform3Bit[(pix1 >> 4) & 0x07][k] << 4) | (waveform3Bit[pix2 & 0x07][k] << 2) | (waveform3Bit[(pix2 >> 4) & 0x07][k] << 0);
      pixel2 = (waveform3Bit[pix3 & 0x07][k] << 6) | (waveform3Bit[(pix3 >> 4) & 0x07][k] << 4) | (waveform3Bit[pix4 & 0x07][k] << 2) | (waveform3Bit[(pix4 >> 4) & 0x07][k] << 0);

      send = ((pixel & B00000011) << 4) | (((pixel & B00001100) >> 2) << 18) | (((pixel & B00010000) >> 4) << 23) | (((pixel & B11100000) >> 5) << 25);
      hscan_start_(send);
      send = ((pixel2 & B00000011) << 4) | (((pixel2 & B00001100) >> 2) << 18) | (((pixel2 & B00010000) >> 4) << 23) | (((pixel2 & B11100000) >> 5) << 25);
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());

      for (int j = 0; j < (this->get_width_internal() / 8) - 1; j++) {
        pix1 = this->buffer_[pos--];
        pix2 = this->buffer_[pos--];
        pix3 = this->buffer_[pos--];
        pix4 = this->buffer_[pos--];
        pixel = (waveform3Bit[pix1 & 0x07][k] << 6) | (waveform3Bit[(pix1 >> 4) & 0x07][k] << 4) | (waveform3Bit[pix2 & 0x07][k] << 2) | (waveform3Bit[(pix2 >> 4) & 0x07][k] << 0);
        pixel2 = (waveform3Bit[pix3 & 0x07][k] << 6) | (waveform3Bit[(pix3 >> 4) & 0x07][k] << 4) | (waveform3Bit[pix4 & 0x07][k] << 2) | (waveform3Bit[(pix4 >> 4) & 0x07][k] << 0);

        send = ((pixel & B00000011) << 4) | (((pixel & B00001100) >> 2) << 18) | (((pixel & B00010000) >> 4) << 23) | (((pixel & B11100000) >> 5) << 25);
        GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
        GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());

        send = ((pixel2 & B00000011) << 4) | (((pixel2 & B00001100) >> 2) << 18) | (((pixel2 & B00010000) >> 4) << 23) | (((pixel2 & B11100000) >> 5) << 25);
        GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
        GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      }
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      vscan_end_();
    }
    delayMicroseconds(230);
  }
  clean_fast_(2, 1);
  clean_fast_(3, 1);
  vscan_start_();
  eink_off_();
  ESP_LOGV(TAG, "Display3b finished (%dms)", millis() - start_time);
}
bool Inkplate::partial_update_() {
  ESP_LOGV(TAG, "Partial update called");
  unsigned long start_time = millis();
  if (this->greyscale_)
    return false;
  if (this->block_partial_)
    return false;

  this->partial_updates_++;

  uint16_t pos = this->get_buffer_length_() - 1;
  uint32_t send;
  uint8_t data;
  uint8_t diffw, diffb;
  uint32_t n = (this->get_buffer_length_() * 2) - 1;

  for (int i = 0; i < this->get_height_internal(); i++) {
    for (int j = 0; j < (this->get_width_internal() / 8); j++) {
      diffw = (this->buffer_[pos] ^ this->partial_buffer_[pos]) & ~(this->partial_buffer_[pos]);
      diffb = (this->buffer_[pos] ^ this->partial_buffer_[pos]) & this->partial_buffer_[pos];
      pos--;
      this->partial_buffer_2_[n--] = LUTW[diffw >> 4] & LUTB[diffb >> 4];
      this->partial_buffer_2_[n--] = LUTW[diffw & 0x0F] & LUTB[diffb & 0x0F];
    }
  }
  ESP_LOGV(TAG, "Partial update buffer built after (%dms)", millis() - start_time);

  eink_on_();
  for (int k = 0; k < 5; k++) {
    vscan_start_();
    n = (this->get_buffer_length_() * 2) - 1;
    for (int i = 0; i < this->get_height_internal(); i++) {
      data = this->partial_buffer_2_[n--];
      send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
      hscan_start_(send);
      for (int j = 0; j < (this->get_width_internal() / 4) - 1; j++) {
        data = this->partial_buffer_2_[n--];
        send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);
        GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
        GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      }
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      vscan_end_();
    }
    delayMicroseconds(230);
    ESP_LOGV(TAG, "Partial update loop k=%d (%dms)", k, millis() - start_time);
  }
  clean_fast_(2, 2);
  clean_fast_(3, 1);
  vscan_start_();
  eink_off_();

  for (int i = 0; i < this->get_buffer_length_(); i++) {
    this->buffer_[i] = this->partial_buffer_[i];
  }
  ESP_LOGV(TAG, "Partial update finished (%dms)", millis() - start_time);
  return true;
}
void Inkplate::vscan_start_() {
    this->ckv_pin_->digital_write(true);
    delayMicroseconds(7);
    this->spv_pin_->digital_write(false);
    delayMicroseconds(10);
    this->ckv_pin_->digital_write(false);
    delayMicroseconds(0);
    this->ckv_pin_->digital_write(true);
    delayMicroseconds(8);
    this->spv_pin_->digital_write(true);
    delayMicroseconds(10);
    this->ckv_pin_->digital_write(false);
    delayMicroseconds(0);
    this->ckv_pin_->digital_write(true);
    delayMicroseconds(18);
    this->ckv_pin_->digital_write(false);
    delayMicroseconds(0);
    this->ckv_pin_->digital_write(true);
    delayMicroseconds(18);
    this->ckv_pin_->digital_write(false);
    delayMicroseconds(0);
    this->ckv_pin_->digital_write(true);
}
void Inkplate::vscan_write_() {
    this->ckv_pin_->digital_write(false);
    this->le_pin_->digital_write(true);
    this->le_pin_->digital_write(false);
    delayMicroseconds(0);
    this->sph_pin_->digital_write(false);
    this->cl_pin_->digital_write(true);
    this->cl_pin_->digital_write(false);
    this->sph_pin_->digital_write(true);
    this->ckv_pin_->digital_write(true);
}
void Inkplate::hscan_start_(uint32_t d) {
    this->sph_pin_->digital_write(false);
    GPIO.out_w1ts = (d) | (1 << this->cl_pin_->get_pin());
    GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
    this->sph_pin_->digital_write(true);
}
void Inkplate::vscan_end_() {
    this->ckv_pin_->digital_write(false);
    this->le_pin_->digital_write(true);
    this->le_pin_->digital_write(false);
    delayMicroseconds(1);
    this->ckv_pin_->digital_write(true);
}
void Inkplate::clean() {
  ESP_LOGV(TAG, "Clean called");
  unsigned long start_time = millis();

  eink_on_();
  clean_fast_(0, 1);   // White
  clean_fast_(0, 8);   // White to White
  clean_fast_(0, 1);   // White to Black
  clean_fast_(0, 8);   // Black to Black
  clean_fast_(2, 1);   // Black to White
  clean_fast_(1, 10);  // White to White
  ESP_LOGV(TAG, "Clean finished (%dms)", millis() - start_time);
}
void Inkplate::clean_fast_(uint8_t c, uint8_t rep) {
  ESP_LOGV(TAG, "Clean fast called with: (%d, %d)", c, rep);
  unsigned long start_time = millis();

  eink_on_();
  uint8_t data = 0;
  if (c == 0)       // White
    data = B10101010;
  else if (c == 1)  // Black
    data = B01010101;
  else if (c == 2)  // Discharge
    data = B00000000;
  else if (c == 3)  // Skip
    data = B11111111;

  uint32_t send = ((data & B00000011) << 4) | (((data & B00001100) >> 2) << 18) | (((data & B00010000) >> 4) << 23) | (((data & B11100000) >> 5) << 25);

  for (int k = 0; k < rep; k++) {
    vscan_start_();
    for (int i = 0; i < this->get_height_internal(); i++) {
      hscan_start_(send);
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      for (int j = 0; j < this->get_width_internal() / 8; j++) {
        GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
        GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
        GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
        GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      }
      GPIO.out_w1ts = (send) | (1 << this->cl_pin_->get_pin());
      GPIO.out_w1tc = get_data_pin_mask_() | (1 << this->cl_pin_->get_pin());
      vscan_end_();
    }
    delayMicroseconds(230);
  ESP_LOGV(TAG, "Clean fast rep loop %d finished (%dms)", k, millis() - start_time);
  }
  ESP_LOGV(TAG, "Clean fast finished (%dms)", millis() - start_time);
}
void Inkplate::pins_z_state_(){
    this->ckv_pin_->pin_mode(INPUT);
    this->sph_pin_->pin_mode(INPUT);

    this->oe_pin_->pin_mode(INPUT);
    this->gmod_pin_->pin_mode(INPUT);
    this->spv_pin_->pin_mode(INPUT);

    this->display_data_0_pin_->pin_mode(INPUT);
    this->display_data_1_pin_->pin_mode(INPUT);
    this->display_data_2_pin_->pin_mode(INPUT);
    this->display_data_3_pin_->pin_mode(INPUT);
    this->display_data_4_pin_->pin_mode(INPUT);
    this->display_data_5_pin_->pin_mode(INPUT);
    this->display_data_6_pin_->pin_mode(INPUT);
    this->display_data_7_pin_->pin_mode(INPUT);
}
void Inkplate::pins_as_outputs_(){
    this->ckv_pin_->pin_mode(OUTPUT);
    this->sph_pin_->pin_mode(OUTPUT);

    this->oe_pin_->pin_mode(OUTPUT);
    this->gmod_pin_->pin_mode(OUTPUT);
    this->spv_pin_->pin_mode(OUTPUT);

    this->display_data_0_pin_->pin_mode(OUTPUT);
    this->display_data_1_pin_->pin_mode(OUTPUT);
    this->display_data_2_pin_->pin_mode(OUTPUT);
    this->display_data_3_pin_->pin_mode(OUTPUT);
    this->display_data_4_pin_->pin_mode(OUTPUT);
    this->display_data_5_pin_->pin_mode(OUTPUT);
    this->display_data_6_pin_->pin_mode(OUTPUT);
    this->display_data_7_pin_->pin_mode(OUTPUT);
}

}
}
