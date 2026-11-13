#include "Arduino.h"
#include "I2CServo.h"
#include "math.h"

// Lower the timer ticks for finer resolution.
#define SERVO_US_PER_CYCLE          (20000)
#define SERVO_MIN_CYCLE_OFF_US      (50)

I2CServo::I2CServo()
: servoChannel(INVALID_SERVO),
  period_min(MIN_PULSE_WIDTH),
  period_max(MAX_PULSE_WIDTH),
  currentPulseWidth(DEFAULT_PULSE_WIDTH)
{
}

I2CServo::~I2CServo()
{
  // Do we need to detach here? 
}

uint8_t I2CServo::attach(int channel)
{
  return attach(channel, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
}

bool I2CServo::attached()
{
  return (servoChannel != INVALID_SERVO);
}

uint8_t I2CServo::attach(int channel, int min, int max)
{
  if (channel < 0 || channel >= SERVO_MAX_SERVOS) {
    return INVALID_SERVO;
  }

  servoChannel = channel;
  period_min = min;
  period_max = max;
  return servoChannel;
}

void I2CServo::detach()
{
  servoChannel = INVALID_SERVO;
}

void I2CServo::write(int value)
{
  if (value < MIN_PULSE_WIDTH) {
    // Treat values below MIN_PULSE_WIDTH as an angle in degrees; map to this
    // servo's configured pulse range. Larger values are passed straight
    // through to writeMicroseconds() as a pulse width, matching Servo.h.
    if (value < 0) value = 0;
    if (value > 180) value = 180;
    value = map(value, 0, 180, period_min, period_max);
  }
  writeMicroseconds(value);
}

int I2CServo::read()
{
  if(INVALID_SERVO == servoChannel)
    return 0;

  // Map the last-written pulse width back to a 0-180 degree angle
  int angle = map(currentPulseWidth, period_min, period_max, 0, 180);
  return angle;
}

void I2CServo::writeMicroseconds(int microseconds)
{
  if(INVALID_SERVO == servoChannel)
    return;

  microseconds = constrain(microseconds, (int)period_min, (int)period_max);
  currentPulseWidth = microseconds;
 
  // Convert microseconds to 12-bit PWM value (0-4095)
  // Formula: (Microseconds * Frequency * 4096) / 1000000
  // TODO: Figure out why writing the pulse doesn't work when using the 12-bit PWM value.  For now, just send the microseconds value directly to the I2C servo controller.
  uint32_t pulse = (uint32_t)microseconds * SERVO_FREQ * 4096 / 1000000;

  Wire1.beginTransmission(8);
  Wire1.write(servoChannel);

  // Write in big-endian order (highest byte first)
  Wire1.write((byte)((microseconds >> 24) & 0xFF)); // Highest byte
  Wire1.write((byte)((microseconds >> 16) & 0xFF)); // Third byte
  Wire1.write((byte)((microseconds >> 8) & 0xFF));  // Second byte
  Wire1.write((byte)(microseconds & 0xFF));         // Lowest byte
  Wire1.endTransmission();
}

int I2CServo::readMicroseconds()
{
  if(INVALID_SERVO == servoChannel)
    return 0;

  // Return the cached last-written value rather than reading back over I2C:
  // Adafruit_PWMServoDriver::getPWM() defaults to reading the ON register,
  // which writeMicroseconds() always leaves at 0 (the pulse lives in OFF),
  // so a hardware read here would always report 0 regardless of position.
  return currentPulseWidth;
}
