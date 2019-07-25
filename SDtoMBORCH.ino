// Test playing a succession of MIDI files from the SD card.
// Example program to demonstrate the use of the MIDFile library
// Just for fun light up a LED in time to the music.
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms
//  Change pin definitions for specific hardware setup - defined below.



#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <I2C_Anything.h>
#include <Wire.h>
//#define USE_MIDI  1

//Buttons: 
#define clockButt 5U
#define stopButt  6U
#define threeButt  7U
#define twoButt  8U
#define oneButt  19U
#define chimeSwitch 4U

#if USE_MIDI // set up for direct MIDI serial output

#define DEBUG(x)
#define DEBUGX(x)
#define SERIAL_RATE 31250

#else // don't use MIDI to allow printing debug statements



#define DEBUG(x)  Serial.print(x)
#define DEBUGX(x) Serial.print(x, HEX)
#define SERIAL_RATE 57600

#endif // USE_MIDI


// SD chip select pin for SPI comms.
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
#define  SD_SELECT  10

// LED definitions for user indicators
//#define READY_LED     7 // when finished
//#define SMF_ERROR_LED 6 // SMF error
//#define SD_ERROR_LED  5 // SD error
#define BEAT_LED      18 // toggles to the 'beat'

#define WAIT_DELAY    2000 // ms

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define interruptPin 9

int myTrack = 0;
int myChannel = 0;
int myData0 = 0;
int myData1 = 0;
unsigned int tracksBuffer16x8[10] = { 0,0,0,0,0,0,0,0,0,0 }; //tracks 0 - 8 then currentstep then mutes ( used for telling ubit what song is playing, and when no song is playing) 
unsigned int midiTracksBuffer16x8[8];
bool sentAMidiBuffer = false;
bool isSending = false;
unsigned long int timeOutStamp = 0;
unsigned long i2cTimer = 0; //for debug
bool waitingForTimeOut = false;
int isMutedInt = 0;
bool bufferIsReady = false;
unsigned long int prevMidiEvent = 0;
bool STOP = false;
// The files in the tune list should be located on the SD car
// or an error will occur opening the file and the next in the 
// list will be opened (skips errors).
char *tuneList[] =
{
	"bass.mid",
	"test.mid"

	//"LOOPDEMO.MID",  // simplest and shortest file
	//"ELISE.MID",
	//"TWINKLE.MID",
	//"GANGNAM.MID",
	//"FUGUEGM.MID",
	//"POPCORN.MID",
	//"AIR.MID",
	//"PRDANCER.MID",
	//"MINUET.MID",
	//"FIRERAIN.MID",
	//"MOZART.MID",
	//"FERNANDO.MID",
	//"SONATAC.MID",
	//"SKYFALL.MID",
	//"XMAS.MID",
	//"GBROWN.MID",
	//"PROWLER.MID",
	//"IPANEMA.MID",
	//"JZBUMBLE.MID",
};

// These don't play as they need more than 16 tracks but will run if MIDIFile.h is changed
//#define MIDI_FILE  "SYMPH9.MID"		// 29 tracks
//#define MIDI_FILE  "CHATCHOO.MID"		// 17 tracks
//#define MIDI_FILE  "STRIPPER.MID"		// 25 tracks

SdFat	SD;
MD_MIDIFile SMF;

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{

#if USE_MIDI
	if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
	{
		Serial.write(pev->data[0] | pev->channel);
		Serial.write(&pev->data[1], pev->size - 1);
	}
	else
		Serial.write(pev->data, pev->size);
#endif
	//DEBUG("\n");
	//DEBUG(millis());
	//DEBUG("\tM T");
	//DEBUG(pev->track);
	//DEBUG(":  Ch ");
	//DEBUG(pev->channel + 1);
	//DEBUG(" Data ");
	//for (uint8_t i = 0; i<pev->size; i++)
	//{
	//	DEBUG(pev->data[i]);
	//	DEBUG(' ');
	//}

	myTrack = pev->track;
	myChannel = pev->channel;
	myData0 = pev->data[0];
	myData1 = pev->data[1] % 16;
	if (myTrack == 0) {
		handleMidiFileEvent(myChannel, myData0, myData1);
	}
}

void handleMidiFileEvent(int Ch, int data0, int data1) {
	if (Ch < 7) {
		if (data0 == 144) { //is a note on
			if (data1 < 16) {
				DEBUG("DATA1 = ");
				DEBUG(data1);
				int val = 0b0000000000000001 << data1;
				//tracksBuffer16x8[Ch] = tracksBuffer16x8[Ch] | val;
				bitSet(tracksBuffer16x8[Ch], data1);
				bufferIsReady = true;
				DEBUG("BUFFER CH = ");
				DEBUG(Ch);
				DEBUG("   ...   ");
				Serial.println(tracksBuffer16x8[Ch], BIN);
				prevMidiEvent = millis();
			}
		}
	}

	// VERIFY THIS ZIM ZAM CODE!!!
	else if (Ch == 7) {
		tracksBuffer16x8[7] = tracksBuffer16x8[7] | data1;
	}
	else if (Ch == 8) {
		data1 = data1 << 8;
		tracksBuffer16x8[7] = tracksBuffer16x8[7] | data1;
	}
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed through the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
	DEBUG("\nS T");
	DEBUG(pev->track);
	DEBUG(": Data ");
	for (uint8_t i = 0; i < pev->size; i++)
	{
		DEBUGX(pev->data[i]);
		DEBUG(' ');
	}
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
	midi_event ev;

	// All sound off
	// When All Sound Off is received all oscillators will turn off, and their volume
	// envelopes are set to zero as soon as possible.
	ev.size = 0;
	ev.data[ev.size++] = 0xb0;
	ev.data[ev.size++] = 120;
	ev.data[ev.size++] = 0;

	for (ev.channel = 0; ev.channel < 16; ev.channel++)
		midiCallback(&ev);
}

void setup(void)
{
	// Set up LED pins
	pinMode(clockButt, INPUT_PULLUP);
	pinMode(stopButt, INPUT_PULLUP);
	pinMode(threeButt, INPUT_PULLUP);
	pinMode(twoButt, INPUT_PULLUP);
	pinMode(oneButt, INPUT_PULLUP);
	pinMode(chimeSwitch, INPUT_PULLUP);


	//pinMode(READY_LED, OUTPUT);
	//pinMode(SD_ERROR_LED, OUTPUT);
	//pinMode(SMF_ERROR_LED, OUTPUT);
	pinMode(interruptPin, OUTPUT);
	digitalWrite(interruptPin, HIGH);
	Serial.begin(SERIAL_RATE);

	DEBUG("\n[MidiFile Play List]");
	Wire.begin(8);                // join i2c bus with address #8
	Wire.onRequest(requestEvent); // register event

	// Initialize SD
	if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
	{
		DEBUG("\nSD init fail!");
		//digitalWrite(SD_ERROR_LED, HIGH);
		while (true);
	}

	// Initialize MIDIFile
	SMF.begin(&SD);
	SMF.setMidiHandler(midiCallback);
	SMF.setSysexHandler(sysexCallback);

	//digitalWrite(READY_LED, HIGH);
}

bool oldClockButtState = false;
bool oldStopButtState = false;
bool oldOneButtState = false;
bool oldTwoButtState = false;
bool oldThreeButtState = false;
bool oldChimeSwitchState = false;

byte buttStates = 0b00000000;
void checkButts() {
	bool clockButtState = !digitalRead(clockButt);
	bool stopButtState = !digitalRead(stopButt);
	bool oneButtState = !digitalRead(oneButt);
	bool twoButtState = !digitalRead(clockButt);
	bool threeButtState = !digitalRead(clockButt);
	bool chimeSwitchState = !digitalRead(chimeSwitch);

//stop
	if (stopButtState & !oldStopButtState) {
		STOP = true;
		oldStopButtState = stopButtState;
	}
	else if (!stopButtState & oldStopButtState) {
		oldStopButtState = stopButtState;
	}
//one
	if (oneButtState & !oldOneButtState) {
		oldOneButtState = oneButtState;
		//tracksBuffer16x8[9] = 1;

		playTrack(0);
		//Serial.print(" TRACKSBUFFER[9] STATE = ");
		//Serial.println(tracksBuffer16x8[9]);

	}
	else if (!oneButtState & oldOneButtState) {
		oldOneButtState = oneButtState;
	}
//two
	if (twoButtState & !oldTwoButtState) {
		oldTwoButtState = twoButtState;
		//tracksBuffer16x8[9] = 2;

		playTrack(1);
		//Serial.print(" TRACKSBUFFER[9] STATE = ");
		//Serial.println(tracksBuffer16x8[9]);

	}
	else if (!twoButtState & oldTwoButtState) {
		oldTwoButtState = twoButtState;
	}
//three
	if (threeButtState & !oldThreeButtState) {
		oldThreeButtState = threeButtState;
		//tracksBuffer16x8[9] = 3;

		playTrack(2);
		//Serial.print(" TRACKSBUFFER[9] STATE = ");
		//Serial.println(tracksBuffer16x8[9]);

	}
	else if (!threeButtState & oldThreeButtState) {
		oldThreeButtState = threeButtState;
	}

	
	/*
	Serial.print("clkB = ");
	Serial.print(digitalRead(clockButt));
	Serial.print("  stop = ");
	Serial.print(digitalRead(stopButt));
	Serial.print("  thrB = ");
	Serial.print(digitalRead(threeButt));
	Serial.print("  twoB = ");
	Serial.print(digitalRead(twoButt));
	Serial.print("  oneB = ");
	Serial.print(digitalRead(oneButt));
	Serial.print("  chmB = ");
	Serial.print(digitalRead(chimeSwitch));
	Serial.println();
	*/
}

void tickMetronome(void)
// flash a LED to the beat
{
	static uint32_t	lastBeatTime = 0;
	static boolean	inBeat = false;
	uint16_t	beatTime;
	beatTime = 60000 / SMF.getTempo();		// msec/beat = ((60sec/min)*(1000 ms/sec))/(beats/min)
	if (!inBeat)
	{
		if ((millis() - lastBeatTime) >= beatTime)
		{
			lastBeatTime = millis();
			digitalWrite(BEAT_LED, HIGH);
			inBeat = true;
		}
	}
	else
	{
		if ((millis() - lastBeatTime) >= 100)	// keep the flash on for 100ms only
		{
			digitalWrite(BEAT_LED, LOW);
			inBeat = false;
		}
	}
}

long unsigned int checkButtTimer = 0;
int checkButtInterval = 10;
void loop(void)
{

	checkButts();


}

void playTrack(int trackSelect) {
	int  err;
	DEBUG("\nFile: ");
	DEBUG(tuneList[trackSelect]);
	SMF.setFilename(tuneList[trackSelect]);
	err = SMF.load();
	if (err != -1)
	{
		DEBUG("\nSMF load Error ");
		DEBUG(err);
		//digitalWrite(SMF_ERROR_LED, HIGH);
		delay(WAIT_DELAY);
	}
	else
	{
		// play the file
		while (!SMF.isEOF() && !STOP)
		{
			if (checkButtTimer + checkButtInterval < millis()) {
				checkButts();
				checkButtTimer = millis();
			}

			if (SMF.getNextEvent())
				tickMetronome();
			checkI2CTimeOut();
			if ((prevMidiEvent < millis()) && bufferIsReady) { // if there was more than a millisecond since last event this needs to go somewhere else that is called all the time
				sendTracksBuffer();
				bufferIsReady = false;
			}
		}

		// done with track
		SMF.close();
		midiSilence();
		Serial.println("STOPPED!");
		STOP = false;
		//tracksBuffer16x8[9] = 0;
		sendTracksBuffer();
	}
}




void hijackUSBMidiTrackBuffer(byte val, byte slot) {
	if (!waitingForTimeOut) {
		clearMidiTracksBuffer();
		bitSet(midiTracksBuffer16x8[slot], val);				//set corresponding bit in corresponding int in the buffer to be sent
		sendUsbMidiPackage();
	}
}