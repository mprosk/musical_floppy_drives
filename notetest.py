import serial


def note_on(s: serial.Serial, note: int, vel: int = 100, channel: int = 0):
    status = 0x90 | (channel & 0x0F)
    note = note & 0x7F
    vel = vel & 0x7F
    s.write([status, note, vel])

def note_off(s: serial.Serial, note: int, vel: int = 100, channel: int = 0):
    status = 0x80 | (channel & 0x0F)
    note = note & 0x7F
    vel = vel & 0x7F
    s.write([status, note, vel])


if __name__ == '__main__':
    print("Floppy drive test. Ctrl-c to exit")
    with serial.Serial('COM3', 115200) as mega:
        note = None
        while True:
            try:
                next_note = int(input("Enter note number: "))
                if note is not None:
                    note_off(mega, note)
                note = next_note
                note_on(mega, note)
            except ValueError:
                print("Invalid input")
            except KeyboardInterrupt:
                print("Note off", note)
                if note is not None:
                    note_off(mega, note)
                break
                

