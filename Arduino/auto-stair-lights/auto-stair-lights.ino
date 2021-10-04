#include <Adafruit_NeoPixel.h>

// Pin defines
#define NEOPXL_PIN 6     // Digital 6
#define LOWER_PIR_PIN A5 // Analog 5
#define UPPER_PIR_PIN A4 // Analog 4

/**
 * @brief Configurable Constants
 */
#define NUM_PIXLES_PER_STAIR (7)
#define NUM_STAIRS (1)

/**
 * @brief Non-configurable constants
 */
#define NUM_PIXELS (NUM_PIXLES_PER_STAIR * NUM_STAIRS)
#define PIR_ADC_CUTOFF (618 / 2) // Found empirically

// Global variables
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPXL_PIN, NEO_GRB + NEO_KHZ800);
bool lower_pir_motion = false;
bool upper_pir_motion = false;

// Forward declarations
void setStair(uint32_t c, uint8_t stair);
void pollPIR(void);
void allOn(void);
void allOff(void);

void setup()
{
  Serial.begin(9600);
  pixels.show();

  pixels.begin();
}

void loop()
{
  pollPIR(); // Poll the sensors

  if (lower_pir_motion)
  {
    for (int i = 0; i < 255; i++)
    {
      uint32_t white = pixels.Color(i, i, i);
      pixels.fill(white, 0, NUM_PIXELS);
      pixels.show();
      delay(3);
    }

    while (lower_pir_motion)
    {
      pollPIR();
      delay(100);
    }

    allOn();
  }
  else
  {
    for (int i = 255; i >= 0; i--)
    {
      uint32_t white = pixels.Color(i, i, i);
      pixels.fill(white, 0, NUM_PIXELS);
      pixels.show();
      delay(3);
    }

    while (!lower_pir_motion)
    {
      pollPIR();
      delay(100);
    }

    allOff();
  }

  delay(100);
}

/**
 * @brief Set specified stair to specified color
 */
void setStair(uint32_t c, uint8_t stair)
{
  //pixels.fill(c, )
}

/**
 * @brief Poll the PIR sensors, update the global motion booleans
 */
void pollPIR()
{
  lower_pir_motion = (PIR_ADC_CUTOFF < analogRead(LOWER_PIR_PIN));
  upper_pir_motion = (PIR_ADC_CUTOFF < analogRead(UPPER_PIR_PIN));
}

void allOn()
{
  uint32_t white = pixels.Color(255, 255, 255);
  pixels.fill(white, 0, NUM_PIXELS);
  pixels.show();
}

void allOff()
{
  pixels.clear();
  pixels.show();
}
