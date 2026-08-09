#include <Wire.h>
#include <Servo.h>

const uint8_t wire_message_angle = 0x01;        // Command byte for writing servo angle (0-180) to the I2C servo controller
const uint8_t wire_message_microseconds = 0x02; // Command byte for writing servo pulse width in microseconds to the I2C servo controller
const uint8_t wire_message_read = 0x03;         // Command byte for reading servo angle (0-180) from the I2C servo controller
const uint8_t wire_servo_yaw = 0x00;            // Servo index for yaw servo on the I2C controller
const uint8_t wire_servo_pitch = 0x01;          // Servo index for pitch servo on the I2C controller
volatile uint8_t orchestratorResponse = 0;      // Response to be sent back to the orchestrator when it wants to read the servo angles

volatile bool newData = false;
uint8_t pendingYawAngle=0, pendingPitchAngle=0, lastYawAngle=0, lastPitchAngle=0;
uint8_t pendingYawMicroSecs=0, pendingPitchMicroSecs=0, lastYawMicroSecs=0, lastPitchMicroSecs=0;

Servo servoYaw;
Servo servoPitch;

// This function fires automatically when the Master sends data
void receiveEvent(int howMany) {
  // The first byte is always the messageid and the second byte is the motor
  byte messageid = Wire.read();
  byte motor = Wire.read();

  if (wire_message_read == messageid) {
    if(wire_servo_yaw == motor) {
      orchestratorResponse = (uint8_t) servoYaw.read();
    } 
    else {
      orchestratorResponse = (uint8_t) servoPitch.read();
    }
    return;
  }

  if (wire_message_angle == messageid) {
    byte angle = Wire.read();
    if(wire_servo_yaw == motor) {
      pendingYawAngle = angle;
    } 
    else {
      pendingPitchAngle = angle;
    }
    newData = true;
    return;
  }

  if (wire_message_microseconds == messageid) {
    int microSeconds = 0;
    microSeconds += Wire.read() << 8;
    microSeconds += Wire.read();
    if(wire_servo_yaw == motor) {
      pendingYawMicroSecs = microSeconds;
    } 
    else {
      pendingPitchMicroSecs = microSeconds;
    }
    newData = true;
    return;
  }

  // unknown message.  print something and flush the pipe
  Serial.print("Unknown message: ");
  Serial.println(messageid);
  for(int i=0; i<howMany-2; i++) {
    Wire.read();
  }
}

// Runs when the Orchestrator calls Wire.requestFrom(8, ...)
void requestEvent() {
  Wire.write(orchestratorResponse);
  orchestratorResponse = 0;
}

void setup() {
  servoYaw.attach(9);
  servoPitch.attach(10);
  
  Serial.begin(115200);

  Wire.begin(8); // Start I2C as Slave with address 8
  Wire.onReceive(receiveEvent); // Tells it what function to run when data arrives
  Wire.onRequest(requestEvent); // handles orchestrator's request for data
}

void loop() {
  if(!newData)
    return;

  if(lastYawAngle != pendingYawAngle && abs(lastYawAngle-pendingYawAngle)>=1){
    servoYaw.write(pendingYawAngle);
    lastYawAngle = pendingYawAngle;
    Serial.print(F("Yaw angle: ")); Serial.println(pendingYawAngle);
  }

  if(lastPitchAngle != pendingPitchAngle && abs(lastPitchAngle-pendingPitchAngle)>=1) {
    servoPitch.write(pendingPitchAngle);
    lastPitchAngle = pendingPitchAngle;
    Serial.print(F("Pitch angle: ")); Serial.println(pendingPitchAngle);
  }

  if(lastYawMicroSecs != pendingYawMicroSecs){
    servoYaw.writeMicroseconds(pendingYawMicroSecs);
    lastYawMicroSecs = pendingYawMicroSecs;
  }

  if(lastPitchMicroSecs != pendingPitchMicroSecs){
    servoPitch.writeMicroseconds(pendingPitchMicroSecs);
    lastPitchMicroSecs = pendingPitchMicroSecs;
  }

  newData = false;
}

