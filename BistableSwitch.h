#ifndef bistable_switch_h
#define bistable_switch_h

#include "Arduino.h"

class BistableSwitch {
  protected:
    bool switchState; //aktualny stan przelacznika
    bool buttonState; //stan przycisku
    bool lastButtonState; //poprzedni stan przycisku
    long debounceDelay; //opoznienie, zapewniajace ze jedno krótkie nacisniecie przycisku nie zmieni aktualnego stanu przelacznika wiecej niz jeden raz!
    long lastDebounceTime; //czas, ktory uplynal od poprzedniego nacisniecia przycisku

  public:
    BistableSwitch(); //Konstruktor
    bool getState(); 
    void update(bool);
    
};

#endif



