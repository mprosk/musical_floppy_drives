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
#define PIN_DEBUG               (2)

/*=====================================================================*
    Private Defines
 *=====================================================================*/
// The number of floppy voices
#define FLOPPY_COUNT            (2)
// The number of tracks on the stepper
#define FLOPPY_TRACKS           (80)

// Highest MIDI note the floppies can reproduce
#define MAXIMUM_NOTE            (60)

// Number of bits in the DDS phase accumulator
#define DDS_PHASE_ACC_BITS      (32)
// DDS sample rate in Hertz
#define DDS_SAMPLE_RATE_HZ      (40000.0)
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
    uint8_t pin_step;       // Pin number of the STEP pin
    uint8_t pin_dir;        // Pin number of the DIR pin
    uint8_t pin_select;     // Pin number of the SELECT pin
    uint8_t pin_trk0;       // Pin number of the TRACK00 pin

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

// Floppy structs    PINS: ST  DR  DS  T0
static floppy_t floppy0 = { 2,  3, A4, A0};     // Floppy 0 struct
static floppy_t floppy1 = { 4,  5, A5, A1};     // Floppy 1 struct

// Array containing pointers to each floppy_t struct
static floppy_t * const floppies[FLOPPY_COUNT] = {&floppy0, &floppy1};


/*=====================================================================*
    Arduino Hooks
 *=====================================================================*/

void setup()
{
    // Initialize Serial
    Serial.begin(115200);
    
    // Initialize GPIO
    pinMode(PIN_DEBUG, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

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
        floppies[f]->direction = false;
        floppies[f]->note_on = false;
        floppy_home(floppies[f]);
    }

    // Initialize timer1 (16-bit) for 40kHz rate
    TCCR1A = 0;   // set entire TCCR1A register to 0
    TCCR1B = 0;   // same for TCCR1B
    TCNT1  = 0;   //initialize counter value to 0
    // Set compare match register for desired increment
    OCR1A = 399;// = (16MHz) / (40,000Hz * 1 prescale) - 1 (must be <65536)
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

        // Wait until DDS update tick
        while(!dds_tick) {}
        dds_tick = false;

        digitalWrite(PIN_DEBUG, HIGH);

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
        
        // Update DDS state for each active voice
        for (uint8_t f = 0; f < FLOPPY_COUNT; f++)
        {
            floppy_t * floppy = floppies[f];
            if (floppy->note_on)
            {
                // Save the current MSB of the phase accumulator
                uint8_t pre = floppy->phase_acc >> DDS_SHIFT;

                // Update the phase accumulator
                floppy->phase_acc += floppy->tuning_word;
                
                // Check if the MSB of the phase accumulator has changed
                if (((floppy->phase_acc >> DDS_SHIFT) == 1) && (pre == 0))
                {
                    // Check if we need to change directions
                    if ((floppy->track == 0) || (floppy->track == FLOPPY_TRACKS - 1))
                    {
                        // Change directions and set the pin accordingly
                        floppy->direction = !floppy->direction;
                        digitalWrite(floppy->pin_dir, floppy->direction);
                    }
                    
                    // Step the motor
                    digitalWrite(floppy->pin_step, LOW);
                    digitalWrite(floppy->pin_step, HIGH);

                    // Update the track position
                    floppy->track += INCREMENT[floppy->direction];
                }
            }
        }
        digitalWrite(PIN_DEBUG, LOW);
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
    digitalWrite(f->pin_dir, LOW);
    while (digitalRead(f->pin_trk0))
    {
        digitalWrite(f->pin_step, LOW);
        digitalWrite(f->pin_step, HIGH);
        delayMicroseconds(3);
    }
    f->track = 0;
    digitalWrite(f->pin_select, HIGH);
}


/*=====================================================================*
    Private Function Implementations
 *=====================================================================*/
