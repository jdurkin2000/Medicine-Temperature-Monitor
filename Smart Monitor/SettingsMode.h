/*******************************************************************************
 *
 * SettingsMode.h
 *
 * Non-blocking settings user-interface overlay for the six-character LCD.
 * Temperature sampling remains owned by TempSensorMode while this overlay is
 * active.
 *
 ******************************************************************************/

#ifndef SETTINGS_MODE_H_
#define SETTINGS_MODE_H_

#define SETTINGS_SCROLL_STEP_MS 200U

typedef enum
{
    SETTINGS_OPTION_ALARM = 0,
    SETTINGS_OPTION_SCALE,
    SETTINGS_OPTION_VOLUME,
    SETTINGS_OPTION_HISTORY,
    SETTINGS_OPTION_COUNT
} SettingsOption;

void settingsModeInit(void);
void settingsModeEnter(void);
void settingsModeExit(void);
unsigned char settingsModeIsActive(void);
unsigned char settingsModeNeedsTimer(void);
unsigned char settingsModeTimerTick(unsigned int elapsedMs);
void settingsModeNextOption(void);
void settingsModeSelectOption(void);
unsigned char settingsModeServiceDisplay(void);

#endif /* SETTINGS_MODE_H_ */
