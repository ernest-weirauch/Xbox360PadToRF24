//Bi-directional, single-sketch

#include <SPI.h>
#include <RF24.h>
#include <XBOXRECV.h>
#include <Wire.h>				//OLED 128x64 display:
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BistableSwitch.h"

//used pins: SPI(), I2C() + 
#define PIN_CE 8
#define PIN_CSN 7
#define PIN_ROLE 2

#define JOY_DEADZONE 0.25 //martwa strefa 0.5 = 50% itd. - pozycja joysticka, od ktorej zmienia sie stan na inny niz 0
#define JOY_MIN -32768 //minimalny odczyt pozycji joysticka
#define JOY_MAX 32767 //max /\


typedef enum {
	roleTransmitter, roleReceiver
}Role;
Role role;

typedef struct {
	char leftJoyX, leftJoyY; //-127 - -1 | 0 | 1 - 128
	char rightJoyX, rightJoyY;
	uint8_t leftTrigger, rightTrigger;

	bool leftBumper, rightBumper;
	bool A, B, X, Y;

	bool back, start;

	bool leftJoyButton, rightJoyButton;

	bool dPadUp, dPadRight, dPadDown, dPadLeft;
	bool guide;
}ControlPackage;
ControlPackage controllerPackage;

struct Xbox360PadState {
	int leftJoyX, leftJoyY;
	int rightJoyX, rightJoyY;
	int leftTrigger, rightTrigger;

	bool leftBumper, rightBumper;
	bool A, B, X, Y;

	bool back, start;

	bool leftJoyButton, rightJoyButton;

	bool dPadUp, dPadRight, dPadDown, dPadLeft;
	bool guide;
} pad, lastPadState;

typedef struct {
	int vibrationLevel;
	char guideLevel;
}FeedbackPackage;
FeedbackPackage feedbackPackage;


// Radio pipe = addresses on single channel for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL }; //nie jest mo�liwe s�uchanie wi�cej ni� jednej transmisji na pojedynczym pipie (dwa nadajniki m�wi�ce jednocze�nie na tym samym pipie nie mog� by� us�yszane przez odbiornik).

RF24 radio(PIN_CE, PIN_CSN);
unsigned long loopStart = 0;
unsigned long successed = 0;
unsigned long failed = 0;
int ratio = 0;


USB Usb;
XBOXRECV XboxRCV(&Usb);

Adafruit_SSD1306 display(128, 64, &Wire, 4);
unsigned long line = 0;
//template<class T>  Print& operator <<(Print& obj, T arg) {	//print stream, dont forget do display.display(); after every use!!
//	obj.print(arg);
//	return obj;
//} //Operator <<
//#define endl "\n"
struct AfterReturn {  ~AfterReturn() {    display.display(); //refresh display  }};
template<class T>  Print& operator <<(Print& obj, T arg) {	//print stream, dont forget do display.display(); after every use!!
	obj.print(arg);
	AfterReturn displayRefresh;
	return obj; //displayRefresh goin to be destroyed
} //Operator <<
#define endl "\n"

BistableSwitch bistableSwitchLights;

void setup() {
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);	//OLED
	display.clearDisplay();						//flush buffer (adafruit logo given from lib)
	display.setTextSize(1);      // Normal 1:1 pixel scale - w tym trybie wy�wietla 8 wierszy po 21 znak�w
	display.setTextColor(SSD1306_WHITE); // Draw white text
	display.setCursor(0, 0);     // Start at top-left corner
	display.cp437(true);         // Use full 256 char 'Code Page 437' font
	display << "setup() begin" << endl; refreshDisplay();

	//Role setup
	pinMode(PIN_ROLE, INPUT_PULLUP);
	if (digitalRead(PIN_ROLE)) role = roleTransmitter;
	else role = roleReceiver;

	display << "role: ";
	if (role == roleTransmitter) { display << "transmitter" << endl; refreshDisplay(); }
	else display << "receiver" << endl; refreshDisplay();

	//Serial �le wp�ywa na prac� NRF24L01?
	//Serial powoduje jitter PWM (serwa)

	// Setup and configure RF radio
	radio.begin(); // Calling begin() sets up a reasonable set of defaults settings

	if (role == roleTransmitter) {
		radio.openWritingPipe(pipes[0]);
		radio.openReadingPipe(1, pipes[1]);

		if (Usb.Init() == -1) {
			digitalWrite(13, HIGH);
			display << "Usb.Init error" << endl << "Halted!" << endl; refreshDisplay();
			while (1); //halt
		}

	} else {//roleReceiver
		radio.openWritingPipe(pipes[1]);
		radio.openReadingPipe(1, pipes[0]);
	}


	radio.setDataRate(RF24_2MBPS); //RF24_250KBPS, RF24_1MBPS, RF24_2MBPS
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(0x34); //u�yj example\scanner �eby przeskanowa� szum na kana�ach i wybra� najczystszy. (Dane z serial monitor wrzu� na wykres)
	radio.enableDynamicPayloads();
	radio.enableAckPayload();
	radio.setRetries(0, 15);                // Smallest time between retries, max no. of retries
	radio.setAutoAck(true);
	//radio.printDetails();                   // Dump the configuration of the rf unit to stdout (Serial?)
	radio.powerUp();
	radio.startListening();



	pinMode(LED_BUILTIN, OUTPUT);
	blink(LED_BUILTIN, 3, 500);
	display << "setup() end" << endl; refreshDisplay();
}


void loop() {
	//nadajnik
	if (role = roleTransmitter) {
		//USB
		Usb.Task();
		if (XboxRCV.XboxReceiverConnected) {
			if (XboxRCV.Xbox360Connected[0]) {  //pad id (4 possible pads connected simultonauesly)
				pad.leftTrigger = XboxRCV.getButtonPress(L2, 0); //0-255
				pad.rightTrigger = XboxRCV.getButtonPress(R2, 0);

				pad.leftJoyX = XboxRCV.getAnalogHat(LeftHatX, 0);
				pad.rightJoyX = XboxRCV.getAnalogHat(RightHatX, 0);
				pad.leftJoyY = XboxRCV.getAnalogHat(LeftHatY, 0);
				pad.rightJoyY = XboxRCV.getAnalogHat(RightHatY, 0);

				pad.A = XboxRCV.getButtonPress(A, 0);   // jest jeszcze wariant getButtonClick
				pad.B = XboxRCV.getButtonPress(B, 0);
				pad.X = XboxRCV.getButtonPress(X, 0);
				pad.Y = XboxRCV.getButtonPress(Y, 0);

				pad.rightBumper = XboxRCV.getButtonPress(R1, 0);
				pad.leftBumper = XboxRCV.getButtonPress(L1, 0);

				pad.dPadLeft = XboxRCV.getButtonPress(LEFT, 0);
				pad.dPadRight = XboxRCV.getButtonPress(RIGHT, 0);
				pad.dPadUp = XboxRCV.getButtonPress(UP, 0);
				pad.dPadDown = XboxRCV.getButtonPress(DOWN, 0);

				pad.rightJoyButton = XboxRCV.getButtonPress(R3, 0);
				pad.leftJoyButton = XboxRCV.getButtonPress(L3, 0);

				pad.back = XboxRCV.getButtonPress(BACK, 0);
				pad.start = XboxRCV.getButtonPress(START, 0);

				pad.guide = XboxRCV.getButtonPress(XBOX, 0);


				//deadzone fixes
				if (pad.leftJoyX <= JOY_MIN) pad.leftJoyX = -32767; // tu by wystarczylo == ale na wypadek zmiany typow na wieksze
				if (pad.leftJoyY <= JOY_MIN) pad.leftJoyY = -32767;
				if (pad.rightJoyX <= JOY_MIN) pad.rightJoyX = -32767;
				if (pad.rightJoyY <= JOY_MIN) pad.rightJoyY = -32767;

				int dlx = abs(pad.leftJoyX);
				int dly = abs(pad.leftJoyY);
				int drx = abs(pad.rightJoyX);
				int dry = abs(pad.rightJoyY);
				if (dlx < JOY_MAX * JOY_DEADZONE) pad.leftJoyX = 0;
				if (dly < JOY_MAX * JOY_DEADZONE) pad.leftJoyY = 0;
				if (drx < JOY_MAX * JOY_DEADZONE) pad.rightJoyX = 0;
				if (dry < JOY_MAX * JOY_DEADZONE) pad.rightJoyY = 0;


				lastPadState = pad;
			}
		}

		//convert pad readings to radio package - include deadzone fix
		if (pad.leftJoyX != 0) controllerPackage.leftJoyX = map(pad.leftJoyX, JOY_MIN, JOY_MAX, -127, 128);
		else controllerPackage.leftJoyX = 0;

		if (pad.leftJoyY != 0) controllerPackage.leftJoyY = map(pad.leftJoyY, JOY_MIN, JOY_MAX, -127, 128);
		else controllerPackage.leftJoyY = 0;

		if (pad.rightJoyX != 0) controllerPackage.rightJoyX = map(pad.rightJoyX, JOY_MIN, JOY_MAX, -127, 128);
		else controllerPackage.rightJoyX = 0;

		if (pad.rightJoyY != 0) controllerPackage.rightJoyY = map(pad.rightJoyY, JOY_MIN, JOY_MAX, -127, 128);
		else controllerPackage.rightJoyY = 0;

		controllerPackage.leftTrigger = pad.leftTrigger;
		controllerPackage.rightTrigger = pad.rightTrigger;
		controllerPackage.A = pad.A;
		controllerPackage.B = pad.B;
		controllerPackage.X = pad.X;
		//controllerPackage.Y = pad.Y;
		bistableSwitchLights.update(pad.Y);
		controllerPackage.Y = bistableSwitchLights.getState(); //ciagle przesyla 1 jezeli swiatla wlaczone 

		controllerPackage.back = pad.back;
		controllerPackage.start = pad.start;

		controllerPackage.rightBumper = pad.rightBumper;
		controllerPackage.leftBumper = pad.leftBumper;
		controllerPackage.leftJoyButton = pad.leftJoyButton;
		controllerPackage.rightJoyButton = pad.rightJoyButton;

		controllerPackage.dPadLeft = pad.dPadLeft;
		controllerPackage.dPadRight = pad.dPadRight;
		controllerPackage.dPadUp = pad.dPadUp;
		controllerPackage.dPadDown = pad.dPadDown;
		controllerPackage.guide = pad.guide;



		//RF24
		loopStart = millis();

		// First, stop listening so we can talk.
		radio.stopListening();
		if (!radio.write(&controllerPackage, sizeof(controllerPackage))) {
			display << "radio.write error" << endl; refreshDisplay(); refreshDisplay();
		}
		radio.startListening();
		display << "delivery success, t: " << millis() - loopStart << endl; refreshDisplay();


		//FUJ
		while (!radio.available() && (millis() - loopStart) < 10) {} //zawiesza wykonanie na 10s!!!

		if (millis() - loopStart >= 200) {
			failed++;
			display << "failed, timeout: " << millis() - loopStart << endl; refreshDisplay();
		} else {
			//feedback receive
			radio.read(&feedbackPackage, sizeof(feedbackPackage));
			display << "got response" << endl; refreshDisplay();
			successed++;
		}

		//count failed to succeded ratio
		int ratio = 100 * failed / (failed + successed);




	} else { //roleReceiver


	   //  if there is data ready
		if (radio.available()) {
			radio.read(&controllerPackage, sizeof(controllerPackage));

			//!!! prepare feedback package !!!

			// Send the final one back. This way, we don't delay
			// the reply while we wait on serial i/o.
			radio.stopListening();
			radio.write(&feedbackPackage, sizeof(feedbackPackage));
			display << "sent response" << endl; refreshDisplay();


			// Now, resume listening so we catch the next packets.
			radio.startListening();
				  



			// update hardware 

					   
		}

		display << "s/f/r: " << successed << "/" << failed << "/" << ratio << endl;
		

	}

}




void blink(uint8_t pin, uint8_t n, unsigned int t) {
	digitalWrite(pin, LOW);
	for (uint8_t i = 0; i < 2 * n; i++) {
		digitalWrite(pin, !digitalRead(pin));
		delay(t);
	}
}

void refreshDisplay() {
	display.display();
}

//template <typename T> void disp(T out) {}
