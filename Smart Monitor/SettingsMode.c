/*******************************************************************************
 *
 * SettingsMode.c
 *
 * Settings is an LCD overlay rather than an application mode. This lets the
 * DS18B20 conversion, alarm checks, and FRAM logging continue in the
 * background while the user navigates the menu.
 *
 ******************************************************************************/

#include <driverlib.h>
#include "SettingsMode.h"
#include "hal_LCD.h"

#define LCD_CHARACTER_COUNT 6
#define SETTINGS_TEXT_LENGTH 8
#define SETTINGS_FIRST_OFFSET 5
#define SETTINGS_LAST_OFFSET (-(SETTINGS_TEXT_LENGTH - 1))

static const int lcdPositions[LCD_CHARACTER_COUNT] =
{
    pos1, pos2, pos3, pos4, pos5, pos6
};

static const char settingsText[] = "SETTINGS";
static const char alarmText[] = "ALARM";
static const char scaleText[] = "SCALE";
static const char volumeText[] = "VOLUME";

static volatile unsigned char settingsActive;
static volatile unsigned char introScrollActive;
static volatile unsigned char displayUpdateRequested;
static volatile unsigned char selectedOption;
static volatile unsigned int scrollElapsedMs;
static volatile signed char scrollOffset;

static void renderCharacters(const char characters[LCD_CHARACTER_COUNT])
{
    unsigned int index = LCD_CHARACTER_COUNT;

    clearLCD();
    while (index != 0U)
    {
        index--;
        showChar(characters[index], lcdPositions[index]);
    }
}

static void renderScrollWindow(const char *text, unsigned int textLength,
                               signed char offset)
{
    char characters[LCD_CHARACTER_COUNT] = {' ', ' ', ' ', ' ', ' ', ' '};
    unsigned int index = textLength;

    while (index != 0U)
    {
        index--;
        int lcdIndex = (int)offset + (int)index;
        if ((lcdIndex >= 0) && (lcdIndex < LCD_CHARACTER_COUNT))
            characters[(unsigned int)lcdIndex] = text[index];
    }

    renderCharacters(characters);
}

static void renderCenteredText(const char *text, unsigned int textLength)
{
    char characters[LCD_CHARACTER_COUNT] = {' ', ' ', ' ', ' ', ' ', ' '};
    unsigned int start = (LCD_CHARACTER_COUNT - textLength + 1U) >> 1;
    unsigned int index = textLength;

    while (index != 0U)
    {
        index--;
        characters[start + index] = text[index];
    }

    renderCharacters(characters);
}

static void renderSelectedOption(unsigned char option)
{
    switch ((SettingsOption)option)
    {
        case SETTINGS_OPTION_ALARM:
            renderCenteredText(alarmText, 5U);
            break;
        case SETTINGS_OPTION_SCALE:
            renderCenteredText(scaleText, 5U);
            break;
        case SETTINGS_OPTION_VOLUME:
            renderCenteredText(volumeText, 6U);
            break;
        case SETTINGS_OPTION_HISTORY:
            // HISTORY is seven letters; HSTRY is its six-digit LCD label.
            renderCenteredText("HSTRY", 5U);
            break;
        default:
            break;
    }
}

void settingsModeInit(void)
{
    settingsActive = 0;
    introScrollActive = 0;
    displayUpdateRequested = 0;
    selectedOption = SETTINGS_OPTION_ALARM;
    scrollElapsedMs = 0;
    scrollOffset = SETTINGS_FIRST_OFFSET;
}

void settingsModeEnter(void)
{
    selectedOption = SETTINGS_OPTION_ALARM;
    scrollOffset = SETTINGS_FIRST_OFFSET;
    scrollElapsedMs = 0;
    introScrollActive = 1;
    settingsActive = 1;
    displayUpdateRequested = 1;
}

void settingsModeExit(void)
{
    settingsActive = 0;
    introScrollActive = 0;
    scrollElapsedMs = 0;
    displayUpdateRequested = 0;
}

unsigned char settingsModeIsActive(void)
{
    return settingsActive;
}

unsigned char settingsModeNeedsTimer(void)
{
    return settingsActive && introScrollActive;
}

unsigned char settingsModeTimerTick(unsigned int elapsedMs)
{
    if (!settingsActive || !introScrollActive)
        return 0;

    scrollElapsedMs += elapsedMs;
    if (scrollElapsedMs < SETTINGS_SCROLL_STEP_MS)
        return 0;

    scrollElapsedMs -= SETTINGS_SCROLL_STEP_MS;

    if (scrollOffset > SETTINGS_LAST_OFFSET)
    {
        scrollOffset--;
    }
    else
    {
        introScrollActive = 0;
    }

    displayUpdateRequested = 1;
    return 1;
}

void settingsModeNextOption(void)
{
    if (!settingsActive || introScrollActive)
        return;

    selectedOption++;
    if (selectedOption >= SETTINGS_OPTION_COUNT)
        selectedOption = SETTINGS_OPTION_ALARM;

    displayUpdateRequested = 1;
}

void settingsModeSelectOption(void)
{
    /*
     * Option-specific editors are intentionally not implemented yet. Keeping
     * selection as a no-op makes the input behavior ready without changing
     * alarm, scale, volume, or history configuration.
     */
}

unsigned char settingsModeServiceDisplay(void)
{
    unsigned int interruptState;
    unsigned char active;
    unsigned char scrolling;
    unsigned char option;
    signed char offset;

    /* Claim one pending frame atomically; LCD rendering stays in foreground. */
    interruptState = __get_SR_register() & GIE;
    __disable_interrupt();
    if (!displayUpdateRequested)
    {
        if (interruptState)
            __enable_interrupt();
        return 0;
    }

    displayUpdateRequested = 0;
    active = settingsActive;
    scrolling = introScrollActive;
    option = selectedOption;
    offset = scrollOffset;

    if (!active)
    {
        if (interruptState)
            __enable_interrupt();
        return 0;
    }

    /* Keep the six-digit update indivisible with respect to button ISRs. */
    if (scrolling)
        renderScrollWindow(settingsText, SETTINGS_TEXT_LENGTH, offset);
    else
        renderSelectedOption(option);

    if (interruptState)
        __enable_interrupt();
    return 1;
}
