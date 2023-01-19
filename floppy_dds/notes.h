/***********************************************************************
 *  DESCRIPTION
 *      Look-up-table of the frequencies of all 128 MIDI notes
 *
 *  REFERENCES
 *      https://en.wikipedia.org/wiki/MIDI_tuning_standard
 ***********************************************************************/

#if !defined(INC_NOTES_H)
#define INC_NOTES_H


/*=====================================================================*
    Required Header Files
 *=====================================================================*/
#include <arduino.h>
#include <avr/pgmspace.h>

/*=====================================================================*
    Public Defines
 *=====================================================================*/
#define NOTE_COUNT          (128)
#define NOTE_TUNING_HZ      (440.0f)


/*=====================================================================*
    Public Functions
 *=====================================================================*/

/*---------------------------------------------------------------------*
 *  NAME
 *      get_note_freq
 *
 *  DESCRIPTION
 *      Returns the frequency of the given MIDI note number
 * 
 * RETURNS
 *      Frequency of the given note. -1 if note is out of range
 *---------------------------------------------------------------------*/
float get_note_freq(uint8_t note);


#endif /* !defined(INC_NOTES_H) */
