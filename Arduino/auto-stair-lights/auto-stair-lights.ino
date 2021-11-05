#include <Adafruit_NeoPixel.h>

// Pin defines
#define NEOPXL_PIN 6     // Digital 6
#define LOWER_PIR_PIN A5 // Analog 5
#define UPPER_PIR_PIN A4 // Analog 4

/**
 * @brief Configurable Constants
 */
#define NUM_PIXLES_PER_STAIR (1)
#define NUM_STAIRS (7)
// This defines the intensity a given stair will get to before
// the next stair begins to fade up (1-255)
#define SEQUENTIAL_FADE_THRESHOLD (20)

/**
 * @brief Non-configurable constants
 */
#define NUM_PIXELS (NUM_PIXLES_PER_STAIR * NUM_STAIRS)
#define PIR_ADC_CUTOFF (618 / 2) // Found empirically
#define MAX_BRIGHTNESS (200)

// Global variables
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPXL_PIN, NEO_GRB + NEO_KHZ800);
bool lower_pir_motion = false;
bool upper_pir_motion = false;
uint32_t stair_target_color[NUM_STAIRS];
uint8_t stair_brightness[NUM_STAIRS];

// Forward declarations
void fadeUpStairs(uint32_t c);
void setStair(uint32_t c, uint8_t stair);
void incrementStair(uint8_t stair);
uint32_t scaleColor(uint32_t c, uint8_t brightness);
bool doneFadingStair(uint8_t stair);
bool stairAtThreshold(uint8_t stair);
bool stairValid(uint8_t stair);
void pollPIR(void);
void allOn(void);
void allOff(void);

void setup()
{
  Serial.begin(9600);

  // Initialize target color & brightness of 0
  for (uint8_t i = 0; i < NUM_STAIRS; i++)
  {
    stair_target_color[i] = pixels.Color(255, 255, 255);
    stair_brightness[i] = 0;
  }
  stair_target_color[0] = pixels.Color(255, 10, 10);
  stair_target_color[1] = pixels.Color(10, 255, 10);
  stair_target_color[2] = pixels.Color(10, 10, 255);
  // Set all the pixels to off
  pixels.begin();
  pixels.show();
  Serial.println("DEBUG: Init complete!");
}

void loop()
{
  while (Serial.available() == 0)
  {
    delay(100);
  }; // Wait for input
  while (Serial.available())
  {
    Serial.read();
  } // Flush the serial RX buffer
  fadeUpStairs();

  /*
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
  }*/

  while (1)
  {
    delay(100);
  }
}

/**
 * @brief Fades up stairs
 */
void fadeUpStairs()
{
  // The number of stairs that are done fading. Initialized to 0 because
  // when this function is first called we haven't faded anything yet
  uint8_t stairs_complete = 0;
  // The number of the farthest stair that has started fading. Initialized
  // to 1 because we are just about to start fading the first stair.
  uint8_t stair_reached = 0;

  // Keep looping until the last stair is done fading
  while (stairs_complete < NUM_STAIRS)
  {
    // Only loop over the stairs that are actively changing
    for (int i = stairs_complete; i <= stair_reached; i++)
    {
      incrementStair(i);

      // If we are incrementing the last stair currently fading and we aren't at the final stair, check to see
      // if we are at the threshold to kick off the fade for the next stair
      if (i == stair_reached && stair_reached < NUM_STAIRS - 1 && stairAtThreshold(i))
      {
        stair_reached++;
        Serial.print("DEBUG: Starting to fade stair ");
        Serial.println(stair_reached, DEC);
      }

      // If our current stair is done fading increment the number of stairs we've completed
      if (doneFadingStair(i))
      {
        Serial.print("DEBUG: Done fading stair ");
        Serial.println(stairs_complete, DEC);
        stairs_complete++;
      }
    }
    delay(10);

    pixels.show();
  }
  Serial.println("DEBUG: Fade complete!");
}

bool doneFadingStair(uint8_t stair)
{
  if (!stairValid(stair))
  {
    Serial.println("ERROR doneFadingStair(): Attempted to access invalid stair!");
    return false;
  }

  return stair_brightness[stair] == MAX_BRIGHTNESS;
}

bool stairAtThreshold(uint8_t stair)
{
  if (!stairValid(stair))
  {
    Serial.println("ERROR stairAtThreshold(): Attempted to access invalid stair!");
    return false;
  }

  return stair_brightness[stair] == SEQUENTIAL_FADE_THRESHOLD;
}

/**
 * @brief Increment light level at specified stair
 */
void incrementStair(uint8_t stair)
{
  if (!stairValid(stair))
  {
    Serial.println("ERROR: Attempted to increment invalid stair!");
    return;
  }

  // Increment then grab the brightness setting for this stair
  uint8_t brightness = ++stair_brightness[stair];

  // Grab the scaled stair color
  uint32_t c = scaleColor(stair_target_color[stair], brightness);

  // Update the pixels
  pixels.fill(c, stair * NUM_PIXLES_PER_STAIR, NUM_PIXLES_PER_STAIR);
}

uint32_t scaleColor(uint32_t c, uint8_t brightness)
{
  // Split the color into R, G, B channels
  uint8_t R = (c >> 16) & 0xFF;
  uint8_t G = (c >> 8) & 0xFF;
  uint8_t B = c & 0xFF;

  // Scale color channels according to brightness
  R = (R * brightness) / MAX_BRIGHTNESS;
  G = (G * brightness) / MAX_BRIGHTNESS;
  B = (B * brightness) / MAX_BRIGHTNESS;

  return pixels.Color(R, G, B);
}

/**
 * @brief Bounds check on number of stairs
 * @details Useful before accessing stair arrays
 */
bool stairValid(uint8_t stair)
{
  return stair < NUM_STAIRS;
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
