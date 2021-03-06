#include <Adafruit_NeoPixel.h>

// Pin defines
#define NEOPXL_PIN 7            // Digital 7
#define LOWER_PIR_PIN A0        // Analog 0
#define UPPER_PIR_PIN A5        // Analog 5
#define COLOR_CHANGE_BUTTON_PIN // Analog 4

/**
 * @brief Configurable Constants
 */
#define NUM_PIXLES_PER_STAIR (7)
#define NUM_STAIRS (7)
// This defines the brightness a given stair will get to before
// the next stair begins to fade while fading up (1-255)
#define SEQUENTIAL_FADE_UP_THRESHOLD (40)
// This defines the brightness a given stair will get to before
// the next stair begins to fade while fading down (1-255)
#define SEQUENTIAL_FADE_DOWN_THRESHOLD (170)
// The delay between fade steps in MS. Used inside the loop in doFade()
// This effectively changes the speed of the fading
#define FADE_STEP_DELAY_MS (5)
// Max brightness the pixels will fade to
// CAN NOT be set below the fade up or fade down thresholds above
#define MAX_BRIGHTNESS (255)

/**
 * @brief Non-configurable constants
 */
#define NUM_PIXELS (NUM_PIXLES_PER_STAIR * NUM_STAIRS)
#define PIR_ADC_CUTOFF (618 / 2) // Found empirically
#define MIN_BRIGHTNESS (0)

// Global variables
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPXL_PIN, NEO_GRB + NEO_KHZ800);
bool lower_pir_motion = false;           // True if there is motion on the lower PIR
bool upper_pir_motion = false;           // True if there is motion on the upper PIR
bool fading_down_stairs = true;          // True if fade is progressing down the stairs
bool fading_up_brightness = true;        // True if fade is going up in brightness
uint32_t stair_target_color[NUM_STAIRS]; // Stores the target stair colors
uint8_t stair_brightness[NUM_STAIRS];    // Stores the current stair brightness

void setup()
{
  Serial.begin(9600);
  Serial.println("DEBUG: Initializing...");

  // Initialize target color white & brightness of 0
  for (uint8_t i = 0; i < NUM_STAIRS; i++)
  {
    stair_target_color[i] = pixels.Color(MAX_BRIGHTNESS, MAX_BRIGHTNESS, MAX_BRIGHTNESS);
    stair_brightness[i] = 0;
  }

  //  Set all the pixels to off
  pixels.begin();
  pixels.show();
  Serial.println("DEBUG: Init complete!");
}

void loop()
{
  pollPIR(); // Poll the sensors

  if (lower_pir_motion)
  {
    // If there is motion at the bottom of the stairs, start a fade up in brightness going up
    // the stairs
    fading_down_stairs = false;
    fading_up_brightness = true;

    startFade();

    // Wait a bit for the stairs to be traversed
    delay(5000);

    // Wait for there to be no motion on both sensors
    while (lower_pir_motion || upper_pir_motion)
    {
      pollPIR();
      delay(200);
    }

    fading_down_stairs = false;
    fading_up_brightness = false;

    startFade();
  }

  if (upper_pir_motion)
  {
    // If there is motion at the top of the stairs, start a fade up in brightness going down
    // the stairs
    fading_down_stairs = true;
    fading_up_brightness = true;

    startFade();

    // Wait a bit for the stairs to be traversed
    delay(5000);

    // Wait for there to be no motion on both sensors
    while (upper_pir_motion || lower_pir_motion)
    {
      pollPIR();
      delay(200);
    }

    fading_down_stairs = true;
    fading_up_brightness = false;

    startFade();
  }

  delay(100);
}

/**
 * @brief Runs a fade based on the global fade parameters (fading_down_stairs, fading_up_brightness, etc.)
 */
void startFade()
{
  // The number of stairs that are done fading. Initialized to 0 because
  // when this function is first called we haven't faded anything yet
  uint8_t stairs_complete = 0;
  // The number of the farthest stair that has started fading. Initialized
  // to 0 because we are just about to start fading the first stair.
  uint8_t stair_reached = 0;

  // Keep looping until the last stair is done fading
  while (stairs_complete < NUM_STAIRS)
  {
    // Only loop over the stairs that are actively changing
    for (int i = stairs_complete; i <= stair_reached; i++)
    {
      int8_t direction_based_stair = stairBasedOnDirection(i);

      tickStair(direction_based_stair);

      // If we are incrementing the last stair currently fading and we aren't at the final stair, check to see
      // if we are at the threshold to kick off the fade for the next stair
      if (i == stair_reached && stair_reached < NUM_STAIRS - 1 && stairAtThreshold(direction_based_stair))
      {
        stair_reached++;
        Serial.print("DEBUG: Starting to fade stair ");
        Serial.println(stairBasedOnDirection(stair_reached), DEC);
      }

      // If our current stair is done fading increment the number of stairs we've completed
      if (doneFadingStair(direction_based_stair))
      {
        Serial.print("DEBUG: Done fading stair ");
        Serial.println(stairBasedOnDirection(stairs_complete), DEC);
        stairs_complete++;
      }
    }
    // This delay effectively controls the speed of the fade
    delay(FADE_STEP_DELAY_MS);

    pixels.show();
  }
  Serial.println("DEBUG: Fade complete!");
}

/**
 * @brief Given a stair number, returns true if that stair is done fading (has reached it's MAX
 * or MIN brightness depending on fading_up_brightness boolean)
 */
bool doneFadingStair(uint8_t stair)
{
  if (!stairValid(stair))
  {
    Serial.println("ERROR doneFadingStair(): Attempted to access invalid stair! Tried to access stair ");
    Serial.println(stair, DEC);
    return false;
  }

  if (fading_up_brightness)
  {
    return stair_brightness[stair] == MAX_BRIGHTNESS;
  }
  else
  {
    return stair_brightness[stair] == MIN_BRIGHTNESS;
  }
}

/**
 * @brief Given a stair number, returns true if that stair has hit its brightness threshold
 * @details Uses fading_up_brightness boolean to test either the fade up or fade down thresholds
 */
bool stairAtThreshold(uint8_t stair)
{
  if (!stairValid(stair))
  {
    Serial.print("ERROR stairAtThreshold(): Attempted to access invalid stair! Tried to access stair ");
    Serial.println(stair, DEC);
    return false;
  }

  if (fading_up_brightness)
  {
    return stair_brightness[stair] == SEQUENTIAL_FADE_UP_THRESHOLD;
  }
  else
  {
    return stair_brightness[stair] == SEQUENTIAL_FADE_DOWN_THRESHOLD;
  }
}

/**
 * @brief Increment or decrement light level at specified stair
 */
void tickStair(uint8_t stair)
{
  if (!stairValid(stair))
  {
    Serial.print("ERROR: Attempted to increment invalid stair! Tried to increment stair ");
    Serial.println(stair, DEC);
    return;
  }

  uint8_t brightness = MAX_BRIGHTNESS;
  // Modify then return stair brightness based on fade direction
  if (fading_up_brightness)
  {
    // TODO: Bounds check?
    brightness = ++stair_brightness[stair];
  }
  else
  {
    // TODO: Bounds check?
    brightness = --stair_brightness[stair];
  }

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
 * @brief Return the stair that should be fading based on the direction of the fade
 * @details If fading up the stairs, returns the given stair. If fading down the stairs returns the inverse stair on the list
 */
uint8_t stairBasedOnDirection(uint8_t stair)
{
  if (fading_down_stairs)
  {
    return stair;
  }
  else
  {
    int8_t inv_stair = stair - (NUM_STAIRS - 1);

    if (inv_stair < 0)
    {
      inv_stair = -inv_stair;
    }

    return inv_stair;
  }
}

/**
 * @brief Poll the PIR sensors, update the global motion booleans
 */
void pollPIR()
{
  lower_pir_motion = (PIR_ADC_CUTOFF < analogRead(LOWER_PIR_PIN));
  upper_pir_motion = (PIR_ADC_CUTOFF < analogRead(UPPER_PIR_PIN));
}

/**
 * @brief Turn all pixels on to full white
 */
void allOn()
{
  uint32_t white = pixels.Color(MAX_BRIGHTNESS, MAX_BRIGHTNESS, MAX_BRIGHTNESS);
  pixels.fill(white, 0, NUM_PIXELS);
  pixels.show();
}

/**
 * @brief Turn all pixels off
 */
void allOff()
{
  pixels.clear();
  pixels.show();
}

/**
 * @brief demo function used to test stairs
 */
void demo()
{
  Serial.println("Send any character to start next fade (Fade up stairs, up brightness)");
  while (Serial.available() == 0)
  {
    delay(100);
  }; // Wait for input
  while (Serial.available())
  {
    Serial.read();
  } // Flush the serial RX buffer

  fading_down_stairs = true;
  fading_up_brightness = true;

  startFade();

  Serial.println("Send any character to start next fade (Fade up stairs, down brightness)");
  while (Serial.available() == 0)
  {
    delay(100);
  }; // Wait for input
  while (Serial.available())
  {
    Serial.read();
  } // Flush the serial RX buffer

  fading_down_stairs = true;
  fading_up_brightness = false;

  startFade();

  Serial.println("Send any character to start next fade (Fade down stairs, up brightness)");
  while (Serial.available() == 0)
  {
    delay(100);
  }; // Wait for input
  while (Serial.available())
  {
    Serial.read();
  } // Flush the serial RX buffer

  fading_down_stairs = false;
  fading_up_brightness = true;

  startFade();

  Serial.println("Send any character to start next fade (Fade down stairs, down brightness)");
  while (Serial.available() == 0)
  {
    delay(100);
  }; // Wait for input
  while (Serial.available())
  {
    Serial.read();
  } // Flush the serial RX buffer

  fading_down_stairs = false;
  fading_up_brightness = false;

  startFade();
}
