/*
 * main.c
 *
 *  Created on: Oct 8, 2014
 *      Author: Owner
 */

#define F_CPU 16000000UL
#include <stdint.h>
#include <stdlib.h>
#define byte uint8_t
#include <avr/io.h>
#include <avr/interrupt.h>
#include "millis.h"
#include "uart.h"
#include "midi.h"
#include "io.h"

//Idea DinSync mode?
//ARP hold

//trigger gate time:
//infinite gate
//tap gate
//midi clock /3
//midi clock /6

//NOTES/Features
//chan auto detect: first midi chan becomes out chan

int main(void) {
	//setup
	millis_setup();
	io_setup();
	uart_init(31250);
	sei();

	while (1) {
		if (uart_test()) handle_midi();
		trigger_poll();
		read_buttons();
		trigger_poll();
	}
	return 1;
}
