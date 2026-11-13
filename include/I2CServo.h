#ifndef I2CServo_h
#define I2CServo_h

#include <inttypes.h>
#include <Wire.h>

#define MIN_PULSE_WIDTH       544     // the shortest pulse sent to a servo in microseconds
#define MAX_PULSE_WIDTH      2400     // the longest pulse sent to a servo in microseconds
#define DEFAULT_PULSE_WIDTH  1500     // default pulse width when servo is attached in microseconds
#define SERVO_FREQ            50      // Analog servos run at ~50 Hz updates
#define SERVO_MAX_SERVOS       16     // Number of channels available on the PCA9685
#define INVALID_SERVO          SERVO_MAX_SERVOS // Sentinel returned by attach() on failure

class I2CServo
{
public:
  I2CServo();
  ~I2CServo();
  uint8_t attach(int channel);          // attach to the given PCA9685 channel (0-15), returns the channel number or INVALID_SERVO on failure
  uint8_t attach(int channel, int min, int max); // as above but also sets min and max pulse widths (in microseconds) for writes
  void detach();
  void write(int value);             // if value < MIN_PULSE_WIDTH it's treated as an angle (0-180), otherwise as pulse width in microseconds
  void writeMicroseconds(int value); // Write pulse width in microseconds, clamped to this servo's configured min/max
  int read();                        // returns the last written position as an angle between 0 and 180 degrees
  int readMicroseconds();            // returns the last written pulse width in microseconds for this servo
  bool attached();                   // return true if this servo is attached, otherwise false
private:
  uint8_t servoChannel;              // channel number for this servo
  uint32_t period_min;               // minimum pulse width in microseconds
  uint32_t period_max;               // maximum pulse width in microseconds
  uint16_t currentPulseWidth;        // last pulse width written, in microseconds (cached; avoids an I2C read on read())
};

#endif
