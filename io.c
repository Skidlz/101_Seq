/*
 * io.c
 *
 *  Created on: Oct 20, 2014
 *      Author: Owner
 */
#include <stdint.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "io.h"
#include "millis.h"
#include "midi.h"
#ifndef byte
#define byte uint8_t
#endif

//PORT B
#define write_but 0
#define rest_but 1
//2&3 clock div
//4&5 arp setting

//PORT D
//trigger in pin 2
//trigger jack switch pin 3

//SEQ
byte notes[255]; //press
byte tie[255]; //0 reg,1 rest,2 tie, 3 sustain
byte note_count = 0; //seq length
byte note_pos = 0; //playback pos
byte playing = 0; //flag
byte write_mode = 0; //in write mode? flag
byte transpose = 0; //transpose in midi notes

//ARP
byte key_held[64];
byte key_sorted[64];
int arp_pos = 0; //playback pos

//MODE/Status
byte trigger = 0; //trigger in connected flag
byte arp_mode = 0; //off, up, down, u&d
byte clock_count = 0; //24ppqn MIDI clock counter
byte beat_div = 3; //24ppqn/3= 32nd notes
byte gate_mode = 0;
unsigned long gate_time = 0;
int key_count = 0; //keys held
byte active_note = 0xff; //note to release

const unsigned long gate_min = 30; //milliseconds

byte isr_flag = 0;

ISR(INT0_vect){
	PORTC = ~PORTC;
	isr_flag = 1;
}

void io_setup() {
	DDRB = 0x00; //inputs
	PORTB = 0xff; //pull ups
	DDRC = 0xff; //out
	PORTC = 0x00; //off
	DDRD =  0b11110010; //0=input
	PORTD = 0b00001101;

	set_song_pos_ptr(song_pos_ptr);
	trigger = !(PIND & (1<<3)); //trigger jack
	arp_mode = (PINB >> 4) & 0b11; //bits 4&5
	change_mode(arp_mode); //set callbacks
	EIMSK |= (1<<INT0);  //INT0
	EICRA |= (1<<ISC01)|(1<<ISC00); //rising edge
}

void change_mode(byte mode) {
	if (mode > 0) {
		set_key_press(key_press_ARP);
		set_key_release(key_release_ARP);
		set_rt_clock(rt_clock_ARP);
		set_rt_start(rt_start_ARP);
		set_rt_cont(rt_cont_ARP);
		set_rt_stop(rt_stop_ARP);
	} else {
		set_key_press(key_press_SEQ);
		set_key_release(key_release_SEQ);
		set_rt_clock(rt_clock_SEQ);
		set_rt_start(rt_start_SEQ);
		set_rt_cont(rt_cont_SEQ);
		set_rt_stop(rt_stop_SEQ);
	}
	if (trigger){
		set_rt_clock(rt_clock_trig);
		set_rt_start(NULL);
		set_rt_cont(NULL);
		set_rt_stop(NULL);
	}
}

void rt_clock_trig(){
	static unsigned long clock_ts = 0;
	if(clock_ts == 0)clock_ts = millis();
	switch(gate_mode){
	case 2: //MIDI 32nds
		gate_time = (millis() - clock_ts) * 3;
		break;
	case 3: //MIDI 16ths
		gate_time = (millis() - clock_ts) * 6;
	}
	clock_ts = millis();
}

void trigger_poll(){
	static unsigned long on_ts = 0;
	if(trigger){
		if((millis() - on_ts) > gate_time && active_note != 0xff)
			note_off(active_note); //release last note

		if(isr_flag){
			isr_flag = 0;
			byte on_flag = 0;

			if (arp_mode ) {
				if(key_count > 1){
					arp_step();
					on_flag = 1;
				}
			} else if (!write_mode && note_count) {
				playing = 1;
				on_flag = seq_step();
			}

			if(on_flag)	on_ts = millis();
		}
	}
}

void read_buttons() {
	static unsigned long lastDebounceTime = 0; // the last time the output pin was toggled
	const int debounceDelay = 50; // the debounce time; increase if the output flickers
	static byte buttons_old;
	static byte button_stable;
	byte buttons = PINB;

	if (buttons != buttons_old) {
		lastDebounceTime = millis();    // reset the debouncing timer
		buttons_old = buttons;
	} else if ((millis() - lastDebounceTime) > debounceDelay) {
		if ((button_stable & ~(buttons)) & (1 << write_but) && !arp_mode) { //write button
			write_mode = !write_mode;
			PORTC = ~PORTC;
			if (write_mode) {
				note_count = 0; //reset seq length
				note_pos = 0; //reset playback/write pos
			}
		}
		if (button_stable & ~(buttons) & (1 << rest_but)) { //rest button
			if(write_mode){
				tie[note_pos++] = key_count? 3 : 1; //sus/rest
				note_count++;
			} else {
				note_pos = 0; //reset playback/write pos
				playing = 0;
				transpose = 0;
				if (active_note != 0xff) note_off(active_note); //release last note
			}
		}

		gate_mode = (buttons >> 2) & 0b11; //bits 2&3
		gate_time = (gate_mode)? -1:gate_min;//max/min time
		beat_div = (1 << gate_mode) * 3; //bits 2&3
		clock_count %= beat_div;
		byte temp = (buttons >> 4) & 0b11; //bits 4&5
		if (arp_mode != temp) { //mode change?
			if (active_note != 0xff) note_off(active_note); //release last note
			arp_pos = 0;
			arp_mode = temp;
			change_mode(arp_mode); //change midi callbacks
		}

		button_stable = buttons;
	}
}

void sort_keys() {
	for (int i = 0; i < key_count; i++)
		key_sorted[i] = key_held[i];
	byte swap = 1;
	while (swap == 1) { //simple bubble sort
		swap = 0;
		for (int i = 0; i < key_count - 1; i++) {
			if (key_sorted[i] < key_sorted[i + 1]) {
				swap = 1;
				byte temp = key_sorted[i];
				key_sorted[i] = key_sorted[i + 1];
				key_sorted[i + 1] = temp;
			}
		}
	}
}

void key_press_ARP(byte note, byte vol) {
	key_held[key_count++] = note;
	sort_keys();
	if (key_count == 1) { //play first note in realtime
		note_on(note, vol);
		active_note = note;
	}

}

void key_press_SEQ(byte note, byte vol) {
	key_held[key_count++] = note;
	if (write_mode) {
		tie[note_pos++] = key_count > 1 ? 2 : 0; //tie/reg
		notes[note_count++] = note;
		note_on(note, vol); //echo keypresses
	} else if (playing) {
		transpose = note - notes[0]; //calculate transpose
	} else {//not writing or playing
		note_on(note, vol); //echo keypresses
	}
}

void key_release_ARP(byte note) {
	for (int i = 0; i < key_count; i++) { //remove from key_held
		if (key_held[i] == note) {
			for (; i < key_count; i++) key_held[i] = key_held[i + 1]; //fill in the gap
			break;
		}
	}
	key_count--;
	arp_pos %= key_count;
	//stop arp if no keys held
	if (key_count <= 0 && active_note != 0xff) note_off(active_note);
	sort_keys();
}

void key_release_SEQ(byte note) {
	if (!playing) note_off(note); //echo release
	key_count--;
}

void song_pos_ptr(int pos) {
	note_pos = pos * 3 / beat_div;
	note_pos %= note_count;
	clock_count = beat_div - 1;
}

void rt_clock_ARP() {
	if (++clock_count == beat_div) {
		clock_count = 0;
		if (key_count > 1) {
			arp_step();
		}
	}
}

void arp_step(){
	if (active_note != 0xff) note_off(active_note); //release last note
	switch(arp_mode) { //step through arp
	case 1: //up
		active_note = key_sorted[arp_pos];
		arp_pos++;
		arp_pos %= key_count;
		break;
	case 2: //down
		arp_pos--;
		arp_pos = (arp_pos + key_count) % key_count;
		active_note = key_sorted[arp_pos];
		break;
	case 3: {//up and down
		int r = key_count - 1;
		active_note = key_sorted[r - abs(arp_pos % (2 * r) - r)]; //triangle function
		arp_pos++;
		arp_pos %= r * 2;
		break;
	}
	}
	byte vol = 127;
	if (active_note != 0xff) note_on(active_note, vol);
}

void rt_clock_SEQ() {
	if (playing && note_count) {
		if (++clock_count == beat_div) { //don't count clock if not playing
			clock_count = 0;
			seq_step();
		}
	}
}

byte seq_step(){
	byte vol = 127;
	byte on_flag = 0;
	switch (tie[note_pos]) {
	case 0: //regular
		if (active_note != 0xff) note_off(active_note); //release last note
		note_on(notes[note_pos] + transpose, vol);
		active_note = notes[note_pos] + transpose;
		on_flag = 1;
		break;
	case 1: //rest
		if (active_note != 0xff) note_off(active_note); //release last note
		active_note = 0xff; //no last note
		break;
	case 2: //tie
		note_on(notes[note_pos] + transpose, vol); //overlap notes
		if (active_note != 0xff) note_off(active_note); //release last note
		active_note = notes[note_pos]  + transpose;
		on_flag = 1;
		break;
	case 3: //sustain
		break;
	}
	note_pos++;
	note_pos %= note_count;
	return on_flag;
}

void rt_start_ARP() {
	arp_pos = 0; //reset start point
	clock_count = beat_div - 1;
}

void rt_start_SEQ() {
	playing = 1;
	transpose = 0;
	note_pos = 0; //reset start point
	clock_count = beat_div - 1;
}

void rt_cont_ARP() {
	clock_count = beat_div - 1;
}

void rt_cont_SEQ() {
	playing = 1;
	//clock_count = beat_div - 1;
}

void rt_stop_ARP() {
	//if(active_note != 0xff) note_off(active_note); //release last note
}

void rt_stop_SEQ() {
	playing = 0;
	if (active_note != 0xff) note_off(active_note); //release last note
	active_note = 0xff;
}
