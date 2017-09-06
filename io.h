/*
 * io.h
 *
 *  Created on: Oct 20, 2014
 *      Author: Owner
 */
#include <stdint.h>
#ifndef byte
#define byte uint8_t
#endif
#ifndef IO_H_
#define IO_H_

void io_setup();
void trigger_poll();
void read_buttons();
void sort_keys();
void key_press_ARP(byte note, byte vol);
void key_press_SEQ(byte note, byte vol);
void key_release_ARP(byte note);
void key_release_SEQ(byte note);
void song_pos_ptr(int pos);
void rt_clock_trig();
void rt_clock_ARP();
void rt_clock_SEQ();
void rt_start_ARP();
void rt_start_SEQ();
void rt_cont_ARP();
void rt_cont_SEQ();
void rt_stop_ARP();
void rt_stop_SEQ();
byte seq_step();
void arp_step();
void change_mode(byte mode);
#endif /* IO_H_ */
