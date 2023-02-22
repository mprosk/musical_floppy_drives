/***********************************************************************
 *  Cthylla
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


/*=====================================================================*
    Private Defines
 *=====================================================================*/
#define MIDI_MSN_MASK           (0xF0)      // most significant nibble
#define MIDI_CHANNEL_MASK       (0x0F)      // least significant nibble
#define MIDI_7BIT_MASK          (0x7F)      // lower 7 bit mask
#define MIDI_MSB_MASK           (0x80)      // Most significant bit (status/data flag)
#define MIDI_NOTE_OFF           (0x80)      // Note off event
#define MIDI_NOTE_ON            (0x90)      // Note on event
#define MIDI_KEY_PRESSURE       (0xA0)      // Polyphonic key pressure/aftertouch
#define MIDI_CONTROL_CHANGE     (0xB0)      // Control change
#define MIDI_PROGRAM_CHANGE     (0xC0)      // Program change
#define MIDI_CHANNEL_PRESSURE   (0xD0)      // Channel pressure/aftertouch
#define MIDI_PITCH_BEND         (0xE0)      // Pitch bend
#define MIDI_SYSTEM_MESSAGE     (0xF0)      // System Message
#define MIDI_SYSEX              (0xF0)
#define MIDI_EOX                (0xF7)      // End of SysEx


/*=====================================================================*
    Private Data Types
 *=====================================================================*/
/* None */


/*=====================================================================*
    Private Function Prototypes
 *=====================================================================*/
/* None */


/*=====================================================================*
    Private Data
 *=====================================================================*/
/* None */


/*=====================================================================*
    Public Function Implementations
 *=====================================================================*/

/*---------------------------------------------------------------------*
 *  NAME
 *      midi_note_on
 *
 *  DESCRIPTION
 *      Sends the three byte note on event over the UART
 *
 *  RETURNS
 *      None
 *---------------------------------------------------------------------*/
void midi_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
    uint8_t buffer[3];
    channel = channel & MIDI_CHANNEL_MASK;      //
    buffer[0] = channel | MIDI_NOTE_ON;
    buffer[1] = note & MIDI_7BIT_MASK;
    buffer[2] = velocity & MIDI_7BIT_MASK;
    Serial.write(buffer, 3);
}

/*---------------------------------------------------------------------*
 *  NAME
 *      midi_note_off
 *
 *  DESCRIPTION
 *      Sends the three byte note off event over the UART
 *
 *  RETURNS
 *      None
 *---------------------------------------------------------------------*/
void midi_note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
    uint8_t buffer[3];
    channel = channel & MIDI_CHANNEL_MASK;      //
    buffer[0] = channel | MIDI_NOTE_OFF;
    buffer[1] = note & MIDI_7BIT_MASK;
    buffer[2] = velocity & MIDI_7BIT_MASK;
    Serial.write(buffer, 3);
}

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
void midi_all_notes_off(uint8_t channel)
{
    uint8_t buffer[3] = {MIDI_CONTROL_CHANGE | channel, 123, 0};
    Serial.write(buffer, 3);
}

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
void midi_all_notes_off_all(void)
{
    for (uint8_t i = 0; i < MIDI_CHANNEL_COUNT; i++)
    {
        midi_all_notes_off(i);
    }
}

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
uint8_t midi_is_white_key(midi_note_t note)
{
    uint8_t n = note.note % 12;
    if (n > 4)
    {
        return n % 2 == 1;
    }
    else
    {
        return n % 2 == 0;
    }
}

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
 *      RUNNING STATUS
 *          Channel message sets the buffer
 *          SysEx or Common message clears the buffer
 *          Real-time message ignores the buffer
 *
 *  RETURNS
 *      midi_note_event_t indicating if a note on, note off, or no
 *          event occurred.
 *      If an event occurred, the information is written to the struct
 *---------------------------------------------------------------------*/
midi_note_event_t midi_read(midi_note_t * note_struct)
{
    static midi_note_event_t event = EVENT_NONE;
    static uint8_t buf[3];
    static uint8_t byte_count = 0;
    static bool is_sysex = false;

    while (Serial.available())
    {
        // Process the incoming byte
        uint8_t byte = Serial.read();

        // Check if it is a status byte
        if (byte & MIDI_MSB_MASK)
        {
            uint8_t message_type = byte & MIDI_MSN_MASK;
            switch (message_type)
            {
                // CHANNEL MESSAGES
                case MIDI_NOTE_OFF:
                    event = EVENT_NOTE_OFF;
                    byte_count = 1;
                    buf[0] = byte & MIDI_CHANNEL_MASK;      // Channel
                    break;

                case MIDI_NOTE_ON:
                    event = EVENT_NOTE_ON;
                    byte_count = 1;
                    buf[0] = byte & MIDI_CHANNEL_MASK;      // Channel
                    break;

                case MIDI_KEY_PRESSURE:
                case MIDI_CONTROL_CHANGE:
                case MIDI_PROGRAM_CHANGE:
                case MIDI_CHANNEL_PRESSURE:
                case MIDI_PITCH_BEND:
                    // Clear the running status on all other channel messages
                    event = EVENT_NONE;
                    byte_count = 0;
#ifdef MIDI_FORWARD_NON_NOTE
                    Serial.write(byte);
#endif
                    break;

                // SYSTEM MESSAGES
                case MIDI_SYSTEM_MESSAGE:
                    // Clear the running status on SysEx or Common messages
                    if (byte <= 0xF7)
                    {
                        // SYSTEM COMMON MESSAGE
                        event = EVENT_NONE;
                        byte_count = 0;

                        is_sysex = (byte == MIDI_SYSEX);
                        if (is_sysex)
                        {
#ifdef MIDI_FORWARD_SYSEX
                            Serial.write(byte);
#endif
                        }
                        else
                        {
#ifdef MIDI_FORWARD_NON_NOTE
                            Serial.write(byte);
#endif
                        }
                    }
                    else
                    {
                        // SYSTEM REALTIME MESSAGE
#ifdef MIDI_FORWARD_NON_NOTE
                         Serial.write(byte);
#endif
                    }
                    break;

                default:
                    break;
            }
        }

        // It's a data byte
        else
        {
            // Store the data if we're already in the middle of receiving a message
            // Byte count will be zero if the running status was cleared
            if (byte_count)
            {
                buf[byte_count] = byte;
                byte_count++;
            }
            else
            {
                if (is_sysex)
                {
#ifdef MIDI_FORWARD_SYSEX
                    Serial.write(byte);
#endif
                }
                else
                {
#ifdef MIDI_FORWARD_NON_NOTE
                    Serial.write(byte);
#endif
                }
            }
        }

        // Check if we've received enough data for a full note event
        if (byte_count == 3)
        {
            // Copy internal buffer to the input struct
            note_struct->channel = buf[0];
            note_struct->note = buf[1];
            note_struct->velocity = buf[2];
            midi_note_event_t return_event = event;

            // Check if note on with zero velocity
            if ((event == EVENT_NOTE_ON) && (buf[2] == 0))
            {
                // Remap this to a note off event
                return_event = EVENT_NOTE_OFF;
            }

            // Reset the byte count
            // Note events do not clear running status. Preserve the first byte of the buffer
            byte_count = 1;

            // Exiting early here could still leave data in the
            // UART buffer that would have to be processed next loop
            return return_event;
        }
    }
    return EVENT_NONE;
}


/*=====================================================================*
    Private Function Implementations
 *=====================================================================*/
/* None */





