/***********************************************************************
 *  Cthylla
 *
 *  DESCRIPTION
 *      <<TODO>> Tells you what the code in the file does.
 *
 *  REFERENCES
 *      <<TODO>> Requirements Specification
 *      <<TODO>> Software Specification
 ***********************************************************************/

#if !defined(INC_MIDI_H)
#define INC_MIDI_H


/*=====================================================================*
    Required Header Files
 *=====================================================================*/
#include "arduino.h"


/*=====================================================================*
    Public Defines
 *=====================================================================*/
#define MIDI_CHANNEL_COUNT  (16)    // Number of MIDI channels

// If defined, non-note, non-SysEx MIDI data will be forwarded to the MIDI out
// #define MIDI_FORWARD_NON_NOTE

// If defined, SysEx data will will forwarded to the MIDI out
// #define MIDI_FORWARD_SYSEX

/*=====================================================================*
    Public Data Types
 *=====================================================================*/
typedef struct midi_note_t
{
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
}
midi_note_t;

typedef struct midi_chord_t
{
    uint8_t source_note;
    uint8_t velocity;
    uint8_t size;
    uint8_t notes[MIDI_CHANNEL_COUNT];
}
midi_chord_t;

typedef enum midi_note_event_t
{
    EVENT_NONE,
    EVENT_NOTE_ON,
    EVENT_NOTE_OFF
}
midi_note_event_t;


/*=====================================================================*
    Public Functions
 *=====================================================================*/

/*---------------------------------------------------------------------*
 *  NAME
 *      midi_note_on
 *
 *  DESCRIPTION
 *      Sends the MIDI command for a note on event
 *      uint8_t channel: MIDI channel (0-15)
 *      uint8_t note: MIDI note (0-127)
 *      uint8_t velocity: note velocity (0-127)
 *
 *  RETURNS
 *      none
 *---------------------------------------------------------------------*/
void midi_note_on(uint8_t channel, uint8_t note, uint8_t velocity);

/*---------------------------------------------------------------------*
 *  NAME
 *      midi_note_off
 *
 *  DESCRIPTION
 *      Sends the MIDI command for a note off event
 *      uint8_t channel: MIDI channel (0-15)
 *      uint8_t note: MIDI note (0-127)
 *      uint8_t velocity: note release velocity (0-127)
 *
 *  RETURNS
 *      none
 *---------------------------------------------------------------------*/
void midi_note_off(uint8_t channel, uint8_t note, uint8_t velocity);

/*---------------------------------------------------------------------*
 *  NAME
 *      midi_is_white_key
 *
 *  DESCRIPTION
 *      Determines if the given note is a white key
 *
 *  RETURNS
 *      1 if the note is a white key, 0 if it is a black key
 *---------------------------------------------------------------------*/
uint8_t midi_is_white_key(midi_note_t note);

/*---------------------------------------------------------------------*
 *  NAME
 *      read_midi
 * 
 *  DESCRIPTION
 *      Checks the serial port for any incoming MIDI bytes
 *      When enough bytes have been received that evaluate to a 
 *      valid MIDI note on or note off event, the struct is updated
 *      with the information of that note event.
 * 
 *      Any bytes that are not part of a note on or note off event
 *      are automatically forwarded to the MIDI OUT
 * 
 *  RETURNS
 *      midi_note_event_t indicating if a note on, note off, or no
 *          event occurred.
 *      If an event occurred, the information is written to the struct
 *---------------------------------------------------------------------*/
midi_note_event_t midi_read(midi_note_t * note_struct);

/*---------------------------------------------------------------------*
 *  NAME
 *      midi_all_notes_off
 * 
 *  DESCRIPTION
 *      Sends the "All Notes Off" message on the given channel
 * 
 *  RETURNS
 *      None
 *---------------------------------------------------------------------*/
void midi_all_notes_off(uint8_t channel);

/*---------------------------------------------------------------------*
 *  NAME
 *      midi_all_notes_off_all
 * 
 *  DESCRIPTION
 *      Sends the "All Notes Off" message on all 16 channels
 * 
 *  RETURNS
 *      None
 *---------------------------------------------------------------------*/
void midi_all_notes_off_all(void);

#endif /* !defined(INC_MIDI_H) */
