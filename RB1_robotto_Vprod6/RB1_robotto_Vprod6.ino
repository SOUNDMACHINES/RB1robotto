/***************************************************************************************************
soundmachines RB1robotto V.C.M. Voltage Controlled Mouth

production firmware VPROD6 (filtering CV input and pitch just modifies the adder to the analog values)

 Using a Soundgin chip to implement a synthsizer module
 capable of 'singing' by checking GATE and PITCH signals and two more CV
 inputs for Vowel selection (also during the same GATE period, Vowels could be randomized at each 
 GATE rising edge by turning the pot fully CW), Consonants (could be disabled by turning 
 fully CCW the Cons knob or randomized ad each GATE rising edge by turning the pot fully CW))
 If Consonant CV is not used OR the input voltage is less than 25mV (50dec), then the consonant is set by the knob.
 We will take in account only three octaves for pitch.
 The pitch control could be used for changing pitch AND to fine tune the scale.
 When Pot is centered, the tune is standard, should be A440 compliant!
 The module could also be used stand alone, as there is a GATE pushbutton and all the 
 relevant controls (pitch, vowel, consonant)!!!
 
 VOWELS (mapped on the pot and on the CV input (SCALED?????)
 A aa 193
 E e 206
 I i 214
 O or 232
 U ue 244
 
 CONSONANTS (""              ""          "")
 S se 236
 T thh 241
 K ko 218
 C ch 201
 M m 223
 G j 216
 D do 203
 
 DAMN!!!! Soundgin has a non continuous musical notes map.
 We cannot use the map functions to manage the pitch inputs..
 C1 = 16
 B1 = 27
 C2 = 32
 B2 = 43
 C3 = 48
 B3 = 59

  
 Copyright 2009-2012 Davide Mancini (soundmachines).
 
 ***************************************************************************************************/

#include <SoftwareSerial.h>

//Pins definitions: analog
#define PITCH_POT    2
#define VOW_POT      1
#define CONS_POT     0
#define PITCH_CV     3
#define VOW_CV       5
#define CONS_CV      4
//Pins definitions: digital
#define SOUNDGIN_RES 2
#define SOUNDGIN_CTS 3
#define GATE_IN      10
#define TX_SW        9 
#define RX_SW        10 //not used as RX, just as GATE
#define PITCH_CV_INACTIVITY 25 //lower threshold 

int gate_status,old_gate_status;
unsigned int Pitch_Pot_Val,Vowel_Pot_Val,Cons_Pot_Val;  //read knobs
unsigned int Pitch_CV_Val,Vowel_CV_Val,Cons_CV_Val;     //read CVs
unsigned int Pitch_Acc;                                 //accumulator for pitch average
unsigned int Pitch_Pot_Acc;                              //acc for pitch pot average
int Pitch_Correction;
unsigned char vowels[5];        //vowels table (see below to modify)
unsigned char consonants[7];    //consonants table (see below to modify)

int Current_Note,Old_Note;      
int Current_Vowel,Old_Vowel;
int Current_Cons,Old_Cons;
int Play_Cons;
int note_played;
int pitchCorrection;

SoftwareSerial mySerial(RX_SW,TX_SW);  //this is the serial to SOUNDIGN chip, only TX is used

//The following is the cross table to transform 5 octaves of 'midi' notes in Soundgin/Babblebot 
//notes (damn they are non-contiguous!)

const char SoundginNoteMap[60] = {
  16,17,18,19,20,21,22,23,24,25,26,27,
  32,33,34,35,36,37,38,39,40,41,42,43,
  48,49,50,51,52,53,54,55,56,57,58,59,
  64,65,66,67,68,69,70,71,72,73,74,75,
  80,81,82,83,84,85,86,87,88,89,90,91
};  

//SETUP STUFF
void setup()
{
  pinMode(TX_SW,OUTPUT);
  pinMode(RX_SW,INPUT);
  pinMode(SOUNDGIN_RES,OUTPUT);    
  pinMode(SOUNDGIN_CTS,INPUT);     //clear to send
  pinMode(GATE_IN, INPUT);
  pinMode(13, OUTPUT);             //led, not used!
  //digitalWrite(SOUNDGIN_RES,0);    //we found that the SOUNDGIN reset is pretty robust
  mySerial.begin(9600);  //start sw serial for communicating with soundgin
  Serial.begin(38400);   //start hw serial, for debug purposes

  //fill our tables
  vowels[0]=193;
  vowels[1]=206;
  vowels[2]=214;
  vowels[3]=232;
  vowels[4]=244;
  consonants[0]=236;
  consonants[1]=241;
  consonants[2]=218;
  consonants[3]=201;
  consonants[4]=223;
  consonants[5]=216;
  consonants[6]=203;
  //init vars
  gate_status=0;
  old_gate_status=0;
  Current_Note=32;    //C2 note
  Old_Note=31;
  Current_Vowel=0;
  Old_Vowel=0;
  Current_Cons=0;
  Old_Cons=0;
  note_played=0;
  Play_Cons=0;

  delay(200);

  //MAX VOLUME
  mySerial.write(27);
  mySerial.write(1);
  mySerial.write(136);    //0x88
  mySerial.write(127);
  delay(50);

  //MAX SPEED
  mySerial.write(27);   //write
  mySerial.write(1);    //1 byte
  mySerial.write(133);  //at address 0x85
  mySerial.write(250);  //content ! was 127
  delay(50);

  //TRANSITION SPEED
  mySerial.write(27);
  mySerial.write(1);
  mySerial.write(132);  //0x84
  mySerial.write(180);
  delay(50);

  //could test the 'speech delay' parameter, the command is 27,10,XX (see Ginsing/Babblebot manual)

  mySerial.write(250); //STOP voice 

}


//MAIN FIRMWARE
void loop()
{
  gate_status=digitalRead(GATE_IN);
  if(gate_status) //GATE is HIGH, something's going on....
  {
    delay(20); //version4, no effect...
    //read all analog channels
    Pitch_Pot_Acc=analogRead(PITCH_POT);
    Pitch_Pot_Acc+=analogRead(PITCH_POT);
    Pitch_Pot_Acc+=analogRead(PITCH_POT);
    Pitch_Pot_Acc+=analogRead(PITCH_POT);
    Pitch_Pot_Val=Pitch_Pot_Acc>>2;
    Vowel_Pot_Val=analogRead(VOW_POT);
    Cons_Pot_Val=analogRead(CONS_POT);
    Vowel_CV_Val=analogRead(VOW_CV);
    Cons_CV_Val=analogRead(CONS_CV);
    Pitch_Acc=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);
    Pitch_Acc+=analogRead(PITCH_CV);

    Pitch_CV_Val=Pitch_Acc>>4; //16 samples average (noisy input could happen....)


    
    //hand calibrated correction for ADC offsets (Hey, it's a 10bit 'cheap' integrated ADC!)
    
    //Serial.print(Pitch_CV_Val,DEC);
    //Serial.print('.');

    Pitch_Correction=map(Pitch_Pot_Val,0,1023,-50,50);
//------------------------------------------------------------------------------------------------------------------

      //If Pitch_CV is not zero (>50) map the pitch CV into a note and store in Current_Note;
      if(Pitch_CV_Val>=PITCH_CV_INACTIVITY)  //there is something coming in the PITCH CV socket
      {
        //map the note and 'later on' play it
        Current_Note=SoundginNoteMap[(map((Pitch_CV_Val+Pitch_Correction),0,1023,  0,59)+1)];//map five octaves to    Soundgin.   
      }
      else
        Current_Note=Old_Note;  //CV is zero, probably there is nothing to do.... (if this is the startup the note is 32)

//----------------------------------------------------------------------------------------
    //ok, here ends the 'pitch managemnt'
    
    //map the vowel pot into a vowel      
    if(Vowel_Pot_Val>= 1000) //we are in RND vowel mode
    {
      if(old_gate_status==0)
      Current_Vowel=random(0,4);    //change to random vowel on the GATE's rising edge only!
    }
    else 
    {
      if(Vowel_Pot_Val < 50) //we are in EXTernally driven vowel mode
      {
        Current_Vowel=map(Vowel_CV_Val,51,1023,0,5);
        Current_Vowel=constrain(Current_Vowel,0,4);
      }
      else //finally, vowel is determined by the knob (ATTENTION, this will retrigger)
      {
        Current_Vowel=map(Vowel_Pot_Val,51,1023,0,5);
        Current_Vowel=constrain(Current_Vowel,0,4);
      }  
    }  

    //map the consonants pot into a consonant...
    if(Cons_Pot_Val>= 1000) //we are in RND mode
    {
      Current_Cons=random(0,4);
      if(old_gate_status==0)  
        Play_Cons=1;
    }
    else
    {
      if(Cons_Pot_Val <= 50) //we are in OFF mode, so no consonants are played
      {
        Play_Cons=0;
      }
      else
        if(Cons_CV_Val>=50) //we are in externally driven consonants
        {
          Current_Cons=map(Cons_CV_Val,51,999,0,8);
          Current_Cons=constrain(Current_Cons,0,7);
          if(old_gate_status==0)
            Play_Cons=1;
        }
        else //consonanat determined by the knob position
      {    
        Current_Cons=map(Cons_Pot_Val,51,999,0,8);
        Current_Cons=constrain(Current_Cons,0,7);
        if(old_gate_status==0)
          Play_Cons=1;          
      }  
    }  

    //now, if the note has changed and SOUNDGIN is ready, set the note (but don't play it yet!)
    if(Old_Note!=Current_Note) // && !(digitalRead(SOUNDGIN_CTS)))
    {
      note_played=1;
      mySerial.write(250);    //P0 (pause) command, basically stops sounding
      //Serial.println(Current_Note,DEC);
      mySerial.write(27);    //Write Speech Note command: 27 is 'write', 8 is 'speech note', the following is the note
      mySerial.write(8);     
      mySerial.write(Current_Note);
      Old_Note=Current_Note;
    }
    //do we have to play a consonant? 
    if(Play_Cons && (old_gate_status == 0))
    {     
      mySerial.write(consonants[Current_Cons]);
 
    }
    //Now, if it's the case, play the vowel! ;-)
    if((Old_Vowel!=Current_Vowel) || (old_gate_status == 0) || (note_played==1))
    {
      mySerial.write(vowels[Current_Vowel]);
      Old_Vowel=Current_Vowel;  
      if(note_played) note_played=0;
    }
    old_gate_status=gate_status;
    delay(20);  //we can play with this value, it's not critical and could also be ZERO
  } //gate_status=1
  else
    if(old_gate_status) //GATE falling edge: stop the sound, transmit a pause (250)
    {
      mySerial.write(250);    
      old_gate_status=gate_status;
    }
}


