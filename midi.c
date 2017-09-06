#ifndef byte
#define byte uint8_t
#endif
#include <stddef.h>
#include "midi.h"
#include "uart.h"

byte midi_chan = 0xff; //MIDI channel
void (*key_press)(byte, byte);
void (*key_release)(byte);
void (*song_pos_ptr)(int);
void (*rt_clock)();
void (*rt_start)();
void (*rt_cont)();
void (*rt_stop)();

void set_key_press(void (*ptr)(byte, byte)){
	key_press = ptr;
}
void set_key_release(void (*ptr)(byte)){
	key_release = ptr;
}
void set_song_pos_ptr(void (*ptr)(int)){
	song_pos_ptr = ptr;
}
void set_rt_clock(void (*ptr)()){
	rt_clock = ptr;
}
void set_rt_start(void (*ptr)()){
	rt_start = ptr;
}
void set_rt_cont(void (*ptr)()){
	rt_cont = ptr;
}
void set_rt_stop(void (*ptr)()){
	rt_stop = ptr;
}

byte handle_com(byte com, byte arg_cnt, byte *args) {
	switch (com & 0xf0) {
	case 0x90: { //key press
		//byte vol = serial_wait();
		if(arg_cnt < 2)return arg_cnt;
		byte note = args[0];
		byte vel = args[1];
		if(vel > 0){ //on
			if(key_press != NULL) key_press(note, vel);
		} else if(key_release != NULL){ //off
			key_release(note);
		}
		return 0; //zero args remain
	}
	case 0x80: { //key release
		//byte vol = serial_wait();
		if(arg_cnt < 2)return arg_cnt;
		byte note = args[0];
		//byte vel = args[1];
		if(key_release != NULL)key_release(note);
		return 0;
	}
	case 0xb0: {//control
		if(arg_cnt < 2)return arg_cnt;
		byte con = args[0];
		byte val = args[1];
		handle_control(con, val);
		return 0;
	}
	case 0xe0: //pitch bend
	case 0xa0: //Polyphonic Key Pressure (Aftertouch)
		if(arg_cnt < 2)return arg_cnt;
		uart_put(com);
		uart_put(args[0]);
		uart_put(args[1]);
		return 0;

	case 0xc0: //Program Change
	case 0xd0: //Channel Pressure
		if(arg_cnt < 1)return arg_cnt;
		uart_put(com);
		uart_put(args[0]);
		return 0;
	default: //error
		return arg_cnt;
	}
}

byte handle_glob_com(byte com, byte arg_cnt, byte *args){
	switch (com) {
	case 0xf0: //SysEx
		if(arg_cnt == 0)uart_put(com);
		else uart_put(args[arg_cnt-1]);
		return 1;
	case 0xf2: //Song Position Pointer
		if(arg_cnt < 2)return arg_cnt;
		unsigned int pos = args[0] | (args[1] <<7);
		if(song_pos_ptr != NULL)song_pos_ptr(pos);
		//note_pos = value / beat_div % note_count;
		return 0;
	case 0xf1: //MIDI Time Code Quarter Frame
	case 0xf3: //Song Select
		if(arg_cnt < 1)return arg_cnt;
		uart_put(com);
		uart_put(args[0]);
		return 0;
	case 0xf7: //SysEx off
		break;
	case 0xf6: //Tune Request
	default: //Undefined
		return 0;
	}
}

void handle_control(byte param, byte value) {
	switch (param) {
	case 0: //example
		//placeholder
		break;
	}
}

byte skip_com(byte com, byte arg_cnt, byte *args) {
	switch(com & 0xf0){
	case 0x90: //key press
	case 0x80: //key release
	case 0xb0: //control
	case 0xe0: //pitch bend
	case 0xa0: //Polyphonic Key Pressure (Aftertouch)
		if(arg_cnt < 2)return arg_cnt;
		uart_put(com);
		uart_put(args[0]);
		uart_put(args[1]);
		return 0;
	case 0xc0: //Program Change
	case 0xd0: //Channel Pressure
		if(arg_cnt < 1)return arg_cnt;
		uart_put(com);
		uart_put(args[0]);
		return 0;
	default:
		return 0;
	}
}

void handle_midi() {
	static byte last_com = 0x90;
	static byte arg_cnt = 0;
	static byte args[2];
	const int arg_max = 2;
	//function returns remaining args
	byte loop_temp = uart_get();
	if (loop_temp >= 0xf8){
		handle_realtime(loop_temp);
	} else {
		if (loop_temp & 0x80) { //command
			last_com = loop_temp;
			arg_cnt = 0;
		} else if (arg_cnt < arg_max){
			args[arg_cnt++] = loop_temp;
		}

		if (last_com >= 0xf0) {
			arg_cnt = handle_glob_com(last_com, arg_cnt, args); //no channel
		} else {
			if (midi_chan == 0xff) midi_chan = last_com & 0x0f; //define our chan?
			if ((last_com & 0x0f) == midi_chan) arg_cnt = handle_com(last_com, arg_cnt, args); //right chan
			else arg_cnt = skip_com(last_com, arg_cnt, args); //wrong channel
		}
	}
}

void note_on(byte note, byte vol){
	uart_put(0x90 | midi_chan);//note on
	if(note > 127) note = 127;
	uart_put(note);
	uart_put(vol);
}

void note_off(byte note){
	uart_put(0x80 | midi_chan);//note off
	if(note > 127) note = 127;
	uart_put(note);
	uart_put(0);//vol
}

void handle_realtime(byte mess) {
	uart_put(mess); //always echo realtime
	switch (mess) {
	case 0xf8: //Timing Clock
		if(rt_clock != NULL)rt_clock();
		break;
	case 0xfA: //Start Sequence
		if(rt_start != NULL)rt_start();
		break;
	case 0xfB: //Continue Sequence
		if(rt_cont != NULL)rt_cont();
		break;
	case 0xfC: //Stop Sequence
		if(rt_stop != NULL)rt_stop();
		break;
	}
}
