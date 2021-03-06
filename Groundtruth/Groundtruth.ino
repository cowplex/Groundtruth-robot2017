//Add the ADNS2620 Library to the sketch.
#include <adns2620_dual.h>
#include <Wire.h>

static inline int8_t sgn(int val) {
  return (val > 0) - (val < 0); // Thanks, Wouter van Oortmerssen and Lee Salzman
}

#define GROUNDTRUTH_SENSORS 1
#define MAIN 2
#define FRONTSIDE 3
#define GEAR 4
#define SHOOTER 5
#define INTAKE 6
#define PARTY 7
#define PULSE_SPEED 11

#define GROUNDTRUTH_MOTION 1
#define GROUNDTRUTH_IMAGE 2

#define MAIN_LIGHTS_R 9
#define MAIN_LIGHTS_G 10
#define MAIN_LIGHTS_B 11
#define FRONTSIDE_LIGHTS_F 7
#define FRONTSIDE_LIGHTS_R 8
#define GEAR_LIGHTS_L 5
#define GEAR_LIGHTS_R 6
#define SHOOTER_LIGHTS 3
#define INTAKE_LIGHTS 4

//Name the ADNS2620, and tell the sketch which pins are used for communication
ADNS2620_DUAL mouse(12,13,2); //SDA1, SDA2, SCK

//This value will be used to store information from the mouse registers.
struct ADNS2620_Return value;

// Keep track of the active register and any data
// for the i2c communications
uint8_t active_register;
uint8_t active_data[3] = {0,0,0};
/*unsigned byte*/ uint8_t active_pulsing = 0; // Lights that should be pulsing
/*unsigned byte*/ uint8_t pwm_output = 0;
uint8_t pwm_step = 4;
int8_t pwm_direction = 1;

byte return_data_image[324*2];

void setup()
{
  // Set up all available digital pins to be outputs
  for(int i = 3; i < 14; i++)
  {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
    
  //Initialize the ADNS2620
    mouse.begin();
    delay(100);
  //A sync is performed to make sure the ADNS2620 is communicating
    mouse.sync();
  //Put the ADNS2620 into 'always on' mode.
    mouse.write(CONFIGURATION_REG, 0x01);

    Wire.begin(64);                // join i2c bus with address #8
    Wire.onRequest(requestEvent); // register event
    Wire.onReceive(receiveEvent); // register event
    
  //Create a serial output.
   // Serial.begin(9600);

    // Blink pin 13 to show that we're ready
    for(int i = 5; i; i--)
    {
      digitalWrite(13, HIGH);
      delay(100);
      digitalWrite(13, LOW);
      delay(100);
    }
}

void loop()
{
    if(pwm_output < pwm_step && pwm_direction < 0)
      pwm_direction *= -1;
    if(255 - pwm_output < pwm_step && pwm_direction > 0)
      pwm_direction *= -1;

    if(pwm_direction > 0)
      pwm_output += pwm_step;
    else
      pwm_output -= pwm_step;

    for(int i = 7; i; i--)
    {
      if(active_pulsing && 1 << i)
      {
        switch(i)
        {
          case SHOOTER:
            analogWrite(SHOOTER_LIGHTS, pwm_output);
            break;
          case GEAR:
            analogWrite(GEAR_LIGHTS_L, pwm_output);
            analogWrite(GEAR_LIGHTS_R, pwm_output);
            break;
          case PARTY:
            analogWrite(MAIN_LIGHTS_R, pulse_wave(pwm_output,   0, pwm_direction));
            analogWrite(MAIN_LIGHTS_G, pulse_wave(pwm_output, 170, pwm_direction));
            analogWrite(MAIN_LIGHTS_B, pulse_wave(pwm_output, 340, pwm_direction));
            break;
          default:
            break;
        }
      }
    }
    delay(10);    
}

// function that executes whenever data is requested by master
void requestEvent() {
  // Requested mouse return data
  if(active_register == GROUNDTRUTH_SENSORS)
  {
    switch(active_data[0])
    {
      // Requested mouse X-Y-SQUAL data, so return it
      case GROUNDTRUTH_MOTION:
        byte return_data[6];
        value = mouse.read(DELTA_X_REG);
        return_data[0] = value.data_l;
        return_data[3] = value.data_r;
        value = mouse.read(DELTA_Y_REG);
        return_data[1] = value.data_l;
        return_data[4] = value.data_r;
        value = mouse.read(SQUAL_REG);
        return_data[2] = value.data_l;
        return_data[5] = value.data_r;
        Wire.write(return_data, 6);
        break;
        
      // Requested pixel data
      case GROUNDTRUTH_IMAGE:
        if(active_data[1] > 0 && active_data[1] <= 27)
        {
          byte send_data[24];
          int offset = (active_data[1] - 1) * 24;
          for(int i = 0; i < 24; i++)
            send_data[i] = return_data_image[i + offset];
          Wire.write(send_data, 24);
        }
        
        //Wire.write(return_data_image, 324*2);
        break;
      default:
        break;
    }
  }
}

void receiveEvent(int howMany) {
  // Get address
  // Get data (if any) for this register
  char i = 0;
  while (0 < Wire.available())
  {
    active_register = Wire.read();
    while (0 < Wire.available())
    {
      if(i < 3)
        active_data[i] = Wire.read(); // receive byte as a character
      else
        Wire.read(); // Error, eat extra bytes
      
      i++;
    }
  }
//Serial.print(active_register);
//Serial.print(" ");
//Serial.print(active_data[0]);
//Serial.print(" ");
//Serial.print(active_data[1]);
//Serial.print(" ");
//Serial.println(active_data[2]);

  switch(active_register)
  {
    case GROUNDTRUTH_SENSORS:
      if(active_data[0] == GROUNDTRUTH_IMAGE && active_data[1] == 0)
      {
        mouse.write(PIXEL_DATA_REG, 0x01);
        for(int i = 324; i; i--)
        {
          value = mouse.read(PIXEL_DATA_REG);
          return_data_image[324   - i] = value.data_l;
          return_data_image[324*2 - i] = value.data_r;
        }
      }
      break;
    case MAIN: // Main robot light PWM
      analogWrite(MAIN_LIGHTS_R, active_data[0]);
      analogWrite(MAIN_LIGHTS_G, active_data[1]);
      analogWrite(MAIN_LIGHTS_B, active_data[2]);
      break;
    case FRONTSIDE: // Active front-side lights
      if(active_data[0] == 1) // Reversing lights
      {
        digitalWrite(FRONTSIDE_LIGHTS_F, LOW);
        digitalWrite(FRONTSIDE_LIGHTS_R, HIGH);
      }
      else if(active_data[0] == 2) // Lights off
      {
        digitalWrite(FRONTSIDE_LIGHTS_F, LOW);
        digitalWrite(FRONTSIDE_LIGHTS_R, LOW);
      }
      else // Default front side
      {
        digitalWrite(FRONTSIDE_LIGHTS_F, HIGH);
        digitalWrite(FRONTSIDE_LIGHTS_R, LOW);
      }
      break;
    case GEAR: // Gear light illumination
      if(active_data[0] == 1) // Pulse mode
        active_pulsing |= 1 << GEAR;
      else if(active_data[0] == 2) // Intensity mode
      {
        active_pulsing &= 255 - (1 << GEAR);
        analogWrite(GEAR_LIGHTS_L, active_data[1]);
        analogWrite(GEAR_LIGHTS_R, active_data[2]);
      }
      else // Off
      {
        active_pulsing &= 255 - (1 << GEAR);
        analogWrite(GEAR_LIGHTS_L, 0);
        analogWrite(GEAR_LIGHTS_R, 0);
      }
      break;
    case SHOOTER: // Shooter status
      if(active_data[0] == 1) // Aiming, pulse lights
        active_pulsing |= 1 << SHOOTER;
      else if(active_data[0] == 2) // Aim Lock!
      {
        active_pulsing &= 255 - (1 << SHOOTER);
        analogWrite(SHOOTER_LIGHTS, 255);
      }
      else // Not aiming or shooting
      {
        active_pulsing &= 255 - (1 << SHOOTER);
        analogWrite(SHOOTER_LIGHTS, 0);
      }
      break;
    case INTAKE: // Intake status
      if(active_data[0]) // Intaking!
        digitalWrite(INTAKE_LIGHTS, HIGH);
      else // Intake off
        digitalWrite(INTAKE_LIGHTS, LOW);
      break;
    case PARTY:
      if(active_data[0] == 1) // Party Mode! Fade lights around RGB color wheel
        active_pulsing |= 1 << PARTY;
      else
        active_pulsing &= 255 - (1 << PARTY);
      break;
    case PULSE_SPEED:
      pwm_step = active_data[0];
      break;
    default:
      break;
  }
}

// Good god, this function.
/*
 * PWM swings in a sawtooth wave from 0 to 255, controlled by
 * the main loop. We want to use that sawtooth waveform to
 * generate a clipped sawtooth with a third of a period rise,
 * a third at full on and a third falling. Clocking three of
 * these 120 degrees out of phase will pulse through the color
 * wheel once every sawtooth wave period.
 */
uint8_t pulse_wave(uint8_t pwm, uint16_t offset, int8_t slope)
{
  uint16_t out, pwm_long;
  // Convert 0-255 and a slope to an X dimension from 0-510
  pwm_long = (uint16_t)offset + (uint16_t)(slope < 0 ? 510 - pwm : pwm);
  // Compensate for any offset angle
  pwm_long = pwm_long % 510;
  // Convert X dimension to triangle wave position
  pwm_long = (pwm_long > 255 ? 510 - pwm_long : pwm_long);
  // Clip the triangle wave
  out = (pwm_long * 3 / 2) % 255;
  // If we're clipping, out will overflow and be lower that pwm
  if(out < pwm_long)
    return 255;
  return (uint8_t) out;
} 
/*uint8_t pulse_wave(uint8_t pwm, uint8_t offset, int8_t slope)
{
  uint16_t out;
  // Convert 0-255 and a slope to an X dimension from 0-510
  uint16_t pwm_long = pwm + (slope < 0 ? 255 - pwm : 0);
  // Amplify the triangle wave
  out = (pwm_long + offset) * 3 / 2;
  // Generate the falling edge if necessary
  if(out % (255*2) >= 255)
    out = 255 - out;
  // Constrain output
  out = out % 255;
  // If we're clipping, out will overflow and be lower that pwm
  if(out < pwm)
    return 255;
  //if(out > 255)
  //  return 255;
  return (uint8_t) out;
}*/

