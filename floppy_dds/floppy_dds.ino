/***********************************************************************
 *  Project Reference <<TODO>>
 *
 *  DESCRIPTION
 *      <<TODO>> Tells you what the code in the file does or refers to
 *      accompanying header file.
 *
 *  REFERENCES
 *      <<TODO>> Requirements Specification
 *      <<TODO>> Software Specification
 ***********************************************************************/

/*=====================================================================*
    Local Header Files
 *=====================================================================*/
#include "midi.h"
#include "notes.h"


/*=====================================================================*
    Interface Header Files
 *=====================================================================*/
/* None */


/*=====================================================================*
    System-wide Header Files
 *=====================================================================*/
/* None */

/*=====================================================================*
    Pin Defines
 *=====================================================================*/
#define PIN_DEBUG0              (18)
#define PIN_DEBUG1              (19)
#define PIN_DEBUG2              (20)
#define PIN_DEBUG3              (21)

/*=====================================================================*
    Private Defines
 *=====================================================================*/
// The number of floppy voices
#define FLOPPY_COUNT            (4)
// The number of tracks on the stepper
#define FLOPPY_TRACKS           (80)

// Highest MIDI note the floppies can reproduce
#define MAXIMUM_NOTE            (70)

// Number of bits in the DDS phase accumulator
#define DDS_PHASE_ACC_BITS      (32)
// DDS sample rate in Hertz
#define DDS_SAMPLE_RATE_HZ      (10000.0)
// Constant used in tuning word calculation. 2^(Phase acc. width)
#define DDS_2POW                (pow(2, DDS_PHASE_ACC_BITS))
// Bit width of the DDS LUT address space
#define DDS_LUT_BITS            (1)
// The number of bits to shift right to access the DDS LUT index
#define DDS_SHIFT               (DDS_PHASE_ACC_BITS - DDS_LUT_BITS)


/*=====================================================================*
    Private Data Types
 *=====================================================================*/

// Container struct for all data associated with a single floppy
typedef struct floppy_t
{
    const uint8_t pin_step;     // Pin number of the STEP pin
    const uint8_t pin_dir;      // Pin number of the DIR pin
    const uint8_t pin_select;   // Pin number of the SELECT pin
    const uint8_t pin_trk0;     // Pin number of the TRACK00 pin

    bool direction;         // Current direction of the stepper motor
    uint8_t track;          // Current track of the stepper motor

    uint8_t note;           // MIDI note number of the current or last note
    uint8_t channel;        // MIDI channel of the current or last note
    bool note_on;           // Flag indicating if the voice is currently playing
    
    uint32_t phase_acc;     // DDS phase accumulator
    uint32_t tuning_word;   // DDS tuning word
}
floppy_t;


/*=====================================================================*
    Private Function Prototypes
 *=====================================================================*/
/* None */


/*=====================================================================*
    Private Constants
 *=====================================================================*/
// Look-up table for the track increment based on the current direction
static const int8_t INCREMENT[] = {-1, 1};


/*=====================================================================*
    Private Data
 *=====================================================================*/

// Flag indicating that a DDS update should be performed
static volatile bool dds_tick = false;

// Floppy structs    PINS:  STEP  DIR  SEL  TR0
static floppy_t floppy0  = {  6,   7,   8,   9};    // Floppy 0 struct
static floppy_t floppy1  = { 15,  17,  14,  16};    // Floppy 1 struct
static floppy_t floppy2  = { 23,  25,  22,  24};    // Floppy 2 struct
static floppy_t floppy3  = { 26,  28,  27,  29};    // Floppy 3 struct
// static floppy_t floppy4  = { 10,  11,  12,  13};    // Floppy 4 struct
// static floppy_t floppy5  = {  2,   3,   4,   5};    // Floppy 5 struct
// static floppy_t floppy6  = { 31,  33,  30,  32};    // Floppy 6 struct
// static floppy_t floppy7  = { 34,  36,  35,  37};    // Floppy 7 struct
// static floppy_t floppy8  = {A11, A10,  A9,  A8};    // Floppy 8 struct
// static floppy_t floppy9  = {A15, A14, A13, A12};    // Floppy 9 struct
// static floppy_t floppy10 = { 39,  41,  38,  40};    // Floppy 10 struct
// static floppy_t floppy11 = { 42,  44,  43,  45};    // Floppy 11 struct
// static floppy_t floppy12 = { A3,  A2,  A1,  A0};    // Floppy 12 struct
// static floppy_t floppy13 = { A7,  A6,  A5,  A4};    // Floppy 13 struct
// static floppy_t floppy14 = { 47,  49,  46,  48};    // Floppy 14 struct
// static floppy_t floppy15 = { 50,  52,  52,  53};    // Floppy 16 struct

// Array containing pointers to each floppy_t struct
static floppy_t * const floppies[FLOPPY_COUNT] = { 
    &floppy0, &floppy1, &floppy2, &floppy3
    // &floppy4, &floppy5, &floppy6, &floppy7,
    // &floppy8, &floppy9, &floppy10, &floppy11,
    // &floppy12, &floppy13, &floppy14, &floppy15
};


/*=====================================================================*
    Arduino Hooks
 *=====================================================================*/

void setup()
{
    // Initialize Serial
    Serial.begin(31250);
    
    // Initialize GPIO
    pinMode(PIN_DEBUG0, OUTPUT);
    pinMode(PIN_DEBUG1, OUTPUT);
    pinMode(PIN_DEBUG2, OUTPUT);
    pinMode(PIN_DEBUG3, INPUT_PULLUP);

    for (uint8_t f = 0; f < FLOPPY_COUNT; f++)
    {
        pinMode(floppies[f]->pin_step, OUTPUT);
        pinMode(floppies[f]->pin_dir, OUTPUT);
        pinMode(floppies[f]->pin_select, OUTPUT);
        pinMode(floppies[f]->pin_trk0, INPUT_PULLUP);
    }

    // Initialize Floppies
    for (uint8_t f = 0; f < FLOPPY_COUNT; f++)
    {
        floppies[f]->phase_acc = 0;
        floppies[f]->tuning_word = 0;
        floppies[f]->note_on = false;
        floppy_home(floppies[f]);
    }

    // Initialize timer1 (16-bit) for 40kHz rate
    TCCR1A = 0;   // set entire TCCR1A register to 0
    TCCR1B = 0;   // same for TCCR1B
    TCNT1  = 0;   //initialize counter value to 0
    // Set compare match register for desired increment
    OCR1A = 1599;// = (16MHz) / (10,000Hz * 1 prescale) - 1 (must be <65536)
    // Turn on CTC mode
    TCCR1B |= (1 << WGM12);
    // Clear the prescaler setting
    TCCR1B &= ~((1 << CS10) | (1 << CS11) | (1 << CS12));
    // Set Clock Source bits to select prescaler
    TCCR1B |=  (1 << CS10);
    // TCCR1B |=  (1 << CS11);
    // TCCR1B |=  (1 << CS12);

     // Disable timer compare interrupt
    TIMSK1 &= ~(1 << OCIE1A);
}


/*=====================================================================*/

void loop()
{
    bool sustain_on = false;

    // Enable timer compare interrupt
    TIMSK1 |= (1 << OCIE1A);

    // MAIN LOOP
    while (1)
    {
        // Check if the panic button has been pressed
        if (!digitalRead(PIN_DEBUG3))
        {
            all_notes_off();
        }

        digitalWrite(PIN_DEBUG0, HIGH);

        // Check for incoming MIDI notes
        midi_note_t note;
        switch (midi_read(&note))
        {
            case EVENT_NOTE_ON:
                // Make sure the note is within the reproducable range of the floppies
                if (note.note > MAXIMUM_NOTE) { break; }

                // Assign the new note to the first inactive voice
                for (uint8_t f = 0; f < FLOPPY_COUNT; f++)
                {
                    floppy_t * floppy = floppies[f];
                    if (!floppy->note_on)
                    {
                        floppy->note_on = true;
                        floppy->note = note.note;
                        floppy->channel = note.channel;
                        floppy->phase_acc = 0;
                        floppy->tuning_word = dds_tuning_word_note(&note);
                        digitalWrite(floppy->pin_select, LOW);
                        break;
                    }
                    // Unable to assign voice
                }
                break;
            case EVENT_NOTE_OFF:
                // Deactivate the voice playing this note
                for (uint8_t f = 0; f < FLOPPY_COUNT; f++)
                {
                    floppy_t * floppy = floppies[f];
                    if (
                        (floppy->note_on) &&
                        (floppy->note == note.note) &&
                        (floppy->channel == note.channel))
                    {
                        floppy->note_on = false;
                        floppy->phase_acc = 0;
                        digitalWrite(floppy->pin_select, HIGH);
                        break;
                    }
                }
                break;
            // TODO: handle sustain pedal CC
            default:
                break;
        }
        digitalWrite(PIN_DEBUG0, LOW);
        
        
        // Only update DDS state when the tick occurs
        if(!dds_tick) { continue; }
        
        digitalWrite(PIN_DEBUG1, HIGH);
        dds_tick = false;

        // Update DDS state for each active voice
        for (uint8_t f = 0; f < FLOPPY_COUNT; f++)
        {
            floppy_t * floppy = floppies[f];
            if (!floppy->note_on) {continue;}
            
            // Save the current MSB of the phase accumulator
            uint8_t pre = floppy->phase_acc >> DDS_SHIFT;

            // Update the phase accumulator
            floppy->phase_acc += floppy->tuning_word;
            
            // Check if the MSB of the phase accumulator has changed
            if (((floppy->phase_acc >> DDS_SHIFT) == 1) && (pre == 0))
            {
                // Update the track position
                floppy->track += INCREMENT[floppy->direction];
                
                // Step the motor
                digitalWrite(floppy->pin_step, LOW);
                digitalWrite(floppy->pin_step, HIGH);

                // Check if we need to change directions
                if ((floppy->track == 0) || (floppy->track == FLOPPY_TRACKS - 1))
                {
                    // Change directions and set the pin accordingly
                    floppy->direction = !floppy->direction;
                    digitalWrite(floppy->pin_dir, !floppy->direction);
                }
            }
        }
        digitalWrite(PIN_DEBUG1, LOW);
    }
}


/*=====================================================================*
    Interrupt Service Routines
 *=====================================================================*/

/*---------------------------------------------------------------------*
 *  NAME
 *      TIMER1_COMPA ISR
 *
 *  DESCRIPTION
 *      Sets the flag indicating that a
 *      DDS update needs to be performed
 *
 *  DURATION
 *      TODO
 * 
 *  FREQUENCY
 *      40 kHz
 *---------------------------------------------------------------------*/
ISR(TIMER1_COMPA_vect)
{
    dds_tick = true;
}


/*=====================================================================*
    Public Function Implementations
 *=====================================================================*/

/*---------------------------------------------------------------------*
 *  NAME
 *      dds_tuning_word_freq
 *
 *  DESCRIPTION
 *      Calculates the tuning word for the given output frequency
 *
 *  RETURNS
 *      The tuning word to achieve the desired output frequency
 *---------------------------------------------------------------------*/
uint32_t dds_tuning_word_freq(float target_freq)
{
    return (uint32_t)((DDS_2POW * target_freq) / DDS_SAMPLE_RATE_HZ);
}

/*---------------------------------------------------------------------*
 *  NAME
 *      dds_tuning_word_note
 *
 *  DESCRIPTION
 *      Calculates the tuning word for the given MIDI note
 *
 *  RETURNS
 *      The tuning word to achieve the desired note
 *---------------------------------------------------------------------*/
uint32_t dds_tuning_word_note(midi_note_t * note)
{
    return dds_tuning_word_freq(get_note_freq(note->note));
}

/*---------------------------------------------------------------------*
 *  NAME
 *      floppy_home
 *
 *  DESCRIPTION
 *      Moves the heads of the given floppy to track zero
 * 
 *  INPUTS
 *      Index of the the floppy to home
 *
 *  RETURNS
 *      None
 *---------------------------------------------------------------------*/
void floppy_home(floppy_t * f)
{
    digitalWrite(f->pin_select, LOW);
    digitalWrite(f->pin_dir, HIGH); // High DIR bit is towards TRK00
    while (digitalRead(f->pin_trk0))
    {
        digitalWrite(f->pin_step, LOW);
        digitalWrite(f->pin_step, HIGH);
        delay(10);
    }
    f->track = 0;
    f->direction = true;
    digitalWrite(f->pin_dir, LOW);
    digitalWrite(f->pin_select, HIGH);
}


/*---------------------------------------------------------------------*
 *  NAME
 *      all_notes_off
 *
 *  DESCRIPTION
 *      Turns all voices off
 *
 *  RETURNS
 *      None
 *---------------------------------------------------------------------*/
void all_notes_off(void)
{
    for (uint8_t f = 0; f < FLOPPY_COUNT; f++)
    {
        floppy_t * floppy = floppies[f];
        floppy->note_on = false;
        floppy->phase_acc = 0;
        digitalWrite(floppy->pin_select, HIGH);
    }
}

/*=====================================================================*
    Private Function Implementations
 *=====================================================================*/

