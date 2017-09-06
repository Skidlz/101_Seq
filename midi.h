/*
 * midi.h
 *
 *  Created on: May 19, 2014
 *      Author: Owner
 */
#include <stdint.h>
#ifndef byte
#define byte uint8_t
#endif
#ifndef MIDI_H_
#define MIDI_H_
byte handle_com(byte com, byte arg_cnt, byte *args);
byte handle_glob_com(byte com, byte arg_cnt, byte *args);
byte skip_com(byte com, byte arg_cnt, byte *args);
void handle_midi(void);
void handle_control(byte, byte);
void handle_realtime(byte);
void note_on(byte, byte);
void note_off(byte);
void set_key_press(void (*ptr)(byte, byte));
void set_key_release(void (*ptr)(byte));
void set_song_pos_ptr(void (*ptr)(int));
void set_rt_clock(void (*ptr)());
void set_rt_start(void (*ptr)());
void set_rt_cont(void (*ptr)());
void set_rt_stop(void (*ptr)());
#endif /* MIDI_H_ */
