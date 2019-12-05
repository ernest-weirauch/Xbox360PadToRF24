#include "bistableSwitch.h"

BistableSwitch::BistableSwitch() {
  lastButtonState = LOW;
  debounceDelay = 50;
  lastDebounceTime = 0;
  switchState = LOW;
}

void BistableSwitch::update(bool currentButtonState) { //load state of switch
  if (currentButtonState != lastButtonState) { //reset debouncing timer
    lastDebounceTime = millis();
  }

  if ( (millis() - lastDebounceTime) > debounceDelay) {

    if (currentButtonState != buttonState) {
      buttonState = currentButtonState;

      //przelacz przycisk tylko jesli nowy odczyt stanu przycisku wysoki
      if (buttonState) {
        switchState = !switchState;
      }
    }
  }

  lastButtonState = currentButtonState;
}

bool BistableSwitch::getState() {
  return switchState;
}



