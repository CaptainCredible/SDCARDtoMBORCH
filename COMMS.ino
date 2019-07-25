


/*
void sendWire2microBit(int note) {
noteToSend = note;
trackOrNote = false;
digitalWrite(interruptPin, LOW);
isSending = true;
timeOutStamp = millis();
}
void send64BitInt() {
digitalWrite(interruptPin, LOW); //start by telling microbit to request track
isSending = true;
timeOutStamp = millis();
delay(1); //change so there is a timer polled and things can be done in background!
}
void send32BitInt(){
digitalWrite(interruptPin, LOW); //start by telling microbit to request track
isSending = true;
timeOutStamp = millis();
delay(1); //change so there is a timer polled and things can be done in background!
}
void sendTracksBuffer64() {
dataPacket64 = 0;
for (byte i = 0; i < 8; i++ ) {
dataPacket64 = dataPacket64 << 8;
dataPacket64 = dataPacket64 + tracksBuffer[i];
}
digitalWrite(interruptPin, LOW); //start by telling microbit to request track
i2cTimer = micros();
isSending = true;
timeOutStamp = millis();
////////Serial.print("sent sendTracksBuffer interrupt and currentStep is ");
////////Serial.println(currentStep);
//delay(1); //change so there is a timer polled and things can be done in background!
}
void sendWire2microBitTrackAndNote(byte traxx, byte note) {
trackOrNote = true;
noteToSend = note + (traxx << 8);
digitalWrite(interruptPin, LOW); //start by telling microbit to request track
isSending = true;
timeOutStamp = millis();
delay(2); //change so there is a timer polled and things can be done in background!
}
*/

unsigned long i2cFails = 0;
unsigned long successfullI2cs = 0;

void sendTracksBuffer() {
	//Serial.println("tracksBuffer = ");
	//for (int i = 0; i < 10; i++) {
		//Serial.println(tracksBuffer16x8[i]);
	//}
	//Serial.println("");
	digitalWrite(interruptPin, LOW); //start by telling microbit to request track
	//i2cTimer = micros();
	isSending = true;
	timeOutStamp = millis();
	Serial.println("requested i2c");
}

void sendUsbMidiPackage() {
	midiTracksBuffer16x8[8] = 200; //200 shal be the magic number
	sentAMidiBuffer = true; //flag the fact that we are sending midi buffer
	sendTracksBuffer();
}


void debugRequestEvent() {
	i2cTimer = micros() - i2cTimer;
	//////Serial.print("transmission took ");
	//////Serial.print(i2cTimer);
	//////Serial.println(" microseconds!");
	isSending = false;
	digitalWrite(interruptPin, HIGH);
}

void measureI2CSuccessRate() {
	//////Serial.print("i2c OK! ");
	successfullI2cs++;
	//////Serial.print(successfullI2cs);
	//////Serial.print("   fails ");
	//////Serial.println(i2cFails);

}



void requestEvent() {  //this is what happens when the microbit asks for a message
	I2C_writeAnything(tracksBuffer16x8);
	isSending = false;
	digitalWrite(interruptPin, HIGH);
	Serial.println("handled i2c");
	Serial.println(tracksBuffer16x8[0]);
	Serial.println(tracksBuffer16x8[1]);
	Serial.println(tracksBuffer16x8[2]);
	Serial.println();
	//Serial.println(tracksBuffer16x8[9]);
	clearTracksBuffer();
	bufferIsReady = false;
}

void clearTracksBuffer() {
	for (byte i = 0; i < 8; i++) {								//for every channel entry in buffer
		tracksBuffer16x8[i] = 0;							// clear buffer
	}
}

//void clearCommandsBuffer() {
//	for (byte i = 0; i < 8; i++) {								//for every channel entry in buffer
//		commandsBuffer[i] = 0;							// clear buffer
//	}
//}

void clearMidiTracksBuffer() {									//also sets sentAMidiBuffer to false
	for (byte i = 0; i < 8; i++) {								//for every channel entry in buffer
//		midiTracksBuffer16x8[i] = 0;							// clear buffer
	}
	sentAMidiBuffer = false;									//set flag back to normal buffers.
}


#define timeOut  10
void checkI2CTimeOut() {
	if (millis() - timeOutStamp > timeOut && isSending) {
		digitalWrite(interruptPin, HIGH);
		isSending = false;
		//clearTracksBuffer();
		Serial.println(" i2c TIMEOUT! ");
		i2cFails++;
	}
}


void triggerImmediately(byte track, byte note) {
	hijackUSBMidiTrackBuffer(note, track);
}

void sendMutes() {
	if (!waitingForTimeOut) {
		//midiTracksBuffer16x8[9] = isMutedInt;
		sendUsbMidiPackage();
	}
}