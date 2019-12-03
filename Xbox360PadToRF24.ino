//Bi-directional, single-sketch

#include <SPI.h>
#include <RF24.h>
#include <XBOXRECV.h>


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
	uint8_t leftTrigger, rightTriger;

	bool leftButton, rightButton;
	bool A, B, X, Y;
	
	bool back, start;
	
	bool leftJoyButton, rightJoyButton;
	
	bool dPadUp, dPadRight, dPadDown, dPadLeft;
	bool guide;
}ControlPackage;
ControlPackage controllerPackage;

typedef struct {
	int vibrationLevel;
	char guideLevel;
}FeedbackPackage;
FeedbackPackage feedbackPackage;

struct Xbox360PadState {
	int leftJoyX, leftJoyY;
	int rightJoyX, rightJoyY;
	int leftTrigger, rightTriger;

	bool leftButton, rightButton;
	bool A, B, X, Y;

	bool back, start;

	bool leftJoyButton, rightJoyButton;

	bool dPadUp, dPadRight, dPadDown, dPadLeft;
	bool guide;
} pad, lastPadState;


// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

RF24 radio(PIN_CE, PIN_CSN);

USB Usb;
XBOXRECV XboxRCV(&Usb);

void setup() {
	//Role setup
	pinMode(PIN_ROLE, INPUT_PULLUP);
	if (digitalRead(PIN_ROLE)) role = roleTransmitter;
	else role = roleReceiver;

	//Serial Ÿle wp³ywa na pracê NRF24L01?
	//Serial powoduje jitter PWM (serwa)

	// Setup and configure RF radio
	radio.begin(); // Calling begin() sets up a reasonable set of defaults settings

	if (role == roleTransmitter) {
		radio.openWritingPipe(pipes[0]);
		radio.openReadingPipe(1, pipes[1]);

		if (Usb.Init() == -1) {
			digitalWrite(13, HIGH);
			while (1); //halt
		}

	} else {//roleReceiver
		radio.openWritingPipe(pipes[1]);
		radio.openReadingPipe(1, pipes[0]);
	}


	radio.setDataRate(RF24_2MBPS); //RF24_250KBPS, RF24_1MBPS, RF24_2MBPS
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(0x34);
	radio.enableDynamicPayloads();
	radio.enableAckPayload();
	radio.setRetries(0, 15);                // Smallest time between retries, max no. of retries
	radio.setAutoAck(true);

	//radio.printDetails();                   // Dump the configuration of the rf unit to stdout (Serial?)

	radio.powerUp();
	radio.startListening();

}


void loop() {
	static unsigned long loopStart = 0;
	static unsigned int successed = 0;
	static unsigned int failed = 0;
	static int ratio = 0;

	//nadajnik
	if (role = roleTransmitter) {
		//USB
		Usb.Task();
		if (XboxRCV.XboxReceiverConnected) {
			if (XboxRCV.Xbox360Connected[0]) {  //pad id (4 possible pads connected simultonauesly)
				pad.leftTrigger = XboxRCV.getButtonPress(L2, 0); //0-255
				pad.rightTriger = XboxRCV.getButtonPress(R2, 0);

				pad.leftJoyX = XboxRCV.getAnalogHat(LeftHatX, 0);
				pad.rightJoyX = XboxRCV.getAnalogHat(RightHatX, 0);
				pad.leftJoyY = XboxRCV.getAnalogHat(LeftHatY, 0);
				pad.rightJoyY = XboxRCV.getAnalogHat(RightHatY, 0);

				pad.A = XboxRCV.getButtonPress(A, 0);   // jest jeszcze wariant getButtonClick
				pad.B = XboxRCV.getButtonPress(B, 0);
				pad.X = XboxRCV.getButtonPress(X, 0);
				pad.Y = XboxRCV.getButtonPress(Y, 0);

				pad.rightButton = XboxRCV.getButtonPress(R1, 0);
				pad.leftButton = XboxRCV.getButtonPress(L1, 0);

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

		//DOPISZ BRAKUJ¥CE




		//RF24
		loopStart = millis();

		// First, stop listening so we can talk.
		radio.stopListening();
		if (!radio.write(&controllerPackage, sizeof(controllerPackage))) {
			// Serial.println(F("failed."));
		}
		radio.startListening();
		// Serial.println(F("delivery success."));
		// printf("Time: %i ", millis() - loop_start);

		while (!radio.available() && (millis() - loopStart) < 10) {  //co 10s
			// Serial.println(F("waiting."));
		}
		if (millis() - loopStart >= 200) {
			// printf("Failed. Timeout: %i...", millis() - loop_start);
			failed++;
		} else {
			//feedback receive
			radio.read(&feedbackPackage, sizeof(feedbackPackage));
			// Serial.print("Got response ");
			successed++;
		}

		if (failed + successed >= COUNT) {
			int _ratio = 100 * failed / (failed + successed);
			// Serial.print("Time ");
			_startTime = (millis() - _startTime);

			successed = 0;
			failed = 0;
			_startTime = millis();
		}


	} else { //roleReceiver

	   //  if there is data ready
		if (radio.available()) {
			radio.read(&controllerPackage, sizeof(controllerPackage));

			// Send the final one back. This way, we don't delay
			// the reply while we wait on serial i/o.
			radio.stopListening();
			radio.write(&feedbackPackage, sizeof(feedbackPackage));
			// Serial.print("Sent response ");

			// Now, resume listening so we catch the next packets.
			radio.startListening();
			
			




			// update hardware 








		}
	}

}







