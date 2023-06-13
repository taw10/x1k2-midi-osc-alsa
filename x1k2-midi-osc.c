/*
 * x1k2-midi-osc.c
 *
 * (c) 2023 Thomas White <taw@bitwiz.me.uk>
 *
 * X1K2-midi-osc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * X1K2-midi-osc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with X1K2-midi-osc.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <alsa/asoundlib.h>
#include <lo/lo.h>


struct faderpot
{
	int id;
	int cc_val;
	int physical_value;
	int physical_value_known;
	int enabled;
	int pickup_value;
	int has_button_and_led;
	int button;
	int led_green;
	int led_orange;
	int led_red;
	const char *type;
};


struct encoder
{
	int id;
	int cc_val;
	int enabled;
	int fine_button;
	int fine_active;
	int has_led;
	int led_green;
	int led_orange;
	int led_red;
};


struct button
{
	const char *name;
	int note;
	int led_green;
	int led_orange;
	int led_red;
};


struct button buttons[18];
struct encoder encoders[6];
struct faderpot faders[4];
struct faderpot potentiometers[12];
int n_buttons = 18;
int n_encoders = 6;
int n_faders = 4;
int n_potentiometers = 12;


static void init_fader(struct faderpot *fad, int id, int cc_val)
{
	fad->id = id;
	fad->cc_val = cc_val;
	fad->physical_value = 0;
	fad->physical_value_known = 0;
	fad->pickup_value = 0;
	fad->has_button_and_led = 0;
	fad->enabled = 0;
	fad->type = "faders";
}


static void init_potentiometer(struct faderpot *fad, int id, int cc_val,
                               int button)
{
	fad->id = id;
	fad->cc_val = cc_val;
	fad->physical_value = 0;
	fad->physical_value_known = 0;
	fad->pickup_value = 0;
	fad->has_button_and_led = 1;
	fad->button = button;
	fad->led_red = button;
	fad->led_orange = button+36;
	fad->led_green = button+72;
	fad->enabled = 0;
	fad->type = "potentiometers";
}


static void add_faderpot_methods(struct faderpot *fad, lo_server osc_server)
{
	char tmp[256];

	snprintf(tmp, 255, "/x1k2/%s/%i/set-pickup-value", fad->type, fad->id);
	lo_server_add_method(osc_server, tmp, "i", pot_set_pickup_handler, fad);

	snprintf(tmp, 255, "/x1k2/%s/%i/enable", fad->type, fad->id);
	lo_server_add_method(osc_server, tmp, "", pot_enable_handler, fad);

	snprintf(tmp, 255, "/x1k2/%s/%i/disable",  fad->type, fad->id);
	lo_server_add_method(osc_server, tmp, "", pot_enable_handler, fad);
}


static void init_encoder_noled(struct encoder *enc, int id, int cc_val,
                               int fine_button)
{
	enc->id = id;
	enc->cc_val = cc_val;
	enc->fine_button = fine_button;
	enc->fine_active = 0;
	enc->has_led = 0;
}


static void init_encoder(struct encoder *enc, int id, int cc_val,
                         int fine_button)
{
	enc->id = id;
	enc->cc_val = cc_val;
	enc->fine_button = fine_button;
	enc->fine_active = 0;
	enc->has_led = 1;
	enc->led_red = fine_button;
	enc->led_orange = fine_button+36;
	enc->led_green = fine_button+72;
}


static void add_encoder_methods(struct encoder *enc, lo_server osc_server)
{
	char tmp[256];
	snprintf(tmp, 255, "/x1k2/encoders/%i/set-led", enc->id);
	lo_server_add_method(osc_server, tmp, "s", enc_set_led_handler, enc);
}



static void init_button_full(struct button *but, const char *name, int note,
                             int r, int o, int g)
{
	but->name = name;
	but->note = note;
	but->led_red = r;
	but->led_orange = o;
	but->led_green = g;
}


static void init_button(struct button *but, const char *name, int note)
{
	init_button_full(but, name, note, note, note+36, note+72);
}


static void add_button_methods(struct button *but, lo_server osc_server)
{
	char tmp[256];
	snprintf(tmp, 255, "/x1k2/buttons/%s/set-led", but->name);
	lo_server_add_method(osc_server, tmp, "s", button_set_led_handler, but);
}


static void show_help(const char *s)
{
	printf("Syntax: %s [-h] [-d /dev/snd/midiXXXX]\n\n", s);
	printf("MIDI to OSC interface for A&H Xone:K2\n"
	       "\n"
	       " -h, --help              Display this help message.\n"
	       " -d, --device <dev>      MIDI device name.\n");
}


static void error_callback(int num, const char *msg, const char *path)
{
	fprintf(stderr, "liblo error %i (%s) for path %s\n", num, msg, path);
}


static void send_note_on(snd_rawmidi_t *midi_out, int note)
{
	unsigned char sbuf[3];
	ssize_t r;
	sbuf[0] = 0x9e;
	sbuf[1] = note;
	sbuf[2] = 127;
	r = snd_rawmidi_write(midi_out, sbuf, 3);
	if ( r != 3 ) {
		printf("snd_rawmidi_write said %li\n", r);
	}
	usleep(1000);
}


static void send_note_off(snd_rawmidi_t *midi_out, int note)
{
	unsigned char sbuf[3];
	ssize_t r;
	sbuf[0] = 0x8e;
	sbuf[1] = note;
	sbuf[2] = 0;
	r = snd_rawmidi_write(midi_out, sbuf, 3);
	if ( r != 3 ) {
		printf("snd_rawmidi_write said %li\n", r);
	}
	usleep(1000);
}


struct led_callback_data
{
	int red;
	int orange;
	int green;
	snd_rawmidi_t *midi_out;
};


static int led_handler(const char *path, const char *types, lo_arg **argv,
                       int argc, lo_message msg, void *vp)
{
	struct led_callback_data *cb = vp;
	if ( strcmp("red", &argv[0]->s) == 0 ) {
		send_note_on(cb->midi_out, cb->red);
	} else if ( strcmp("orange", &argv[0]->s) == 0 ) {
		send_note_on(cb->midi_out, cb->orange);
	} else if ( strcmp("green", &argv[0]->s) == 0 ) {
		send_note_on(cb->midi_out, cb->green);
	} else if ( strcmp("off", &argv[0]->s) == 0 ) {

		/* Usually, turning off any one of the colours turns off the
		 * LED, regardless of the current colour.  However, the bottom
		 * left button's LED is weird, I think because it's
		 * also the "layer" button.  It can only be switched off
		 * from the same colour.  So, we force it to be red. */
		if ( cb->red == 12 ) {
			send_note_on(cb->midi_out, cb->red);
		}
		send_note_off(cb->midi_out, cb->red);
	} else {
		fprintf(stderr, "Unrecognised LED mode '%s'\n", &argv[0]->s);
	}

	return 1;
}


static int pot_set_pickup_handler(const char *path, const char *types, lo_arg **argv,
                                  int argc, lo_message msg, void *vp)
{
	return 1;
}


static void handle_note_off(int note, int vel, lo_address osc_send_addr)
{
	int i;

	for ( i=0; i<num_fine; i++ ) {
		if ( note == fine_buttons[i] ) {
			fine_vals[i] = 0;
		}
	}
}


static void handle_note(int note, int vel, lo_address osc_send_addr)
{
	int i;

	for ( i=0; i<num_buttons; i++ ) {
		if ( note == buttons[i] ) {
			char tmp[256];
			snprintf(tmp, 255, "/x1k2/buttons/%i",
			         button_numbers[i]);
			lo_send(osc_send_addr, tmp, "");
			printf("sending %s\n", tmp);
		}
	}

	for ( i=0; i<num_fine; i++ ) {
		if ( note == fine_buttons[i] ) {
			fine_vals[i] = 1;
		}
	}
}


static void handle_encoder(int enc, int val, lo_address osc_send_addr)
{
	char tmp[32];
	const char *v;
	int i;
	const char *fine = "";

	if ( val == 1 ) {
		v = "inc";
	} else if ( val == 127 ) {
		v = "dec";
	} else {
		fprintf(stderr, "Invalid encoder value %i\n", val);
		return;
	}

	for ( i=0; i<num_fine; i++ ) {
		if ( enc == fine_encoders[i] ) {
			if ( fine_vals[i] ) {
				fine = "-fine";
			}
		}
	}

	snprintf(tmp, 32, "/x1k2/encoders/%i/%s%s", enc, v, fine);
	printf("sending %s\n", tmp);
	lo_send(osc_send_addr, tmp, "");
}


static void handle_cc(int cc, int val, lo_address osc_send_addr)
{
	char tmp[32];
	const char *type;
	int num;

	if ( cc < 4 ) {
		handle_encoder(cc+1, val, osc_send_addr);
		return;
	} else if ( cc<=15 ) {
		type = "potentiometers";
		num = cc+1;
	} else if ( cc<=19 ) {
		type = "faders";
		num = cc-15;
	} else if ( cc<=21 ) {
		handle_encoder(cc+81, val, osc_send_addr);
		return;
	} else {
		fprintf(stderr, "CC %i unrecognised!\n", cc);
		return;
	}

	snprintf(tmp, 32, "/x1k2/%s/%i", type, num);
	printf("sending %s = %i\n", tmp, val);
	lo_send(osc_send_addr, tmp, "i", val);
}


static size_t process_midi(unsigned char *buf, size_t avail, lo_address osc_send_addr)
{
	if ( avail < 1 ) return 0;

	if ( (buf[0] & 0xf0) == 0x90 ) {
		/* Note on */
		if ( avail < 3 ) return 0;
		handle_note(buf[1], buf[2], osc_send_addr);
		return 3;
	} else if ( (buf[0] & 0xf0) == 0x80 ) {
		/* Note off */
		if ( avail < 3 ) return 0;
		handle_note_off(buf[1], buf[2], osc_send_addr);
		return 3;
	} else if ( (buf[0] & 0xf0) == 0xb0 ) {
		/* CC change */
		if ( avail < 3 ) return 0;
		handle_cc(buf[1], buf[2], osc_send_addr);
		return 3;
	} else {
		printf("Ignoring MIDI command %i\n", buf[0]);
		return 1;
	}
}


static int hup_err(struct pollfd *pfds, int nfds)
{
	int i;
	for ( i=0; i<nfds; i++ ) {
		if ( pfds[i].revents & (POLLERR | POLLHUP) ) return 1;
	}
	return 0;
}


int main(int argc, char *argv[])
{
	int c, r, i;
	char *dev = NULL;
	snd_rawmidi_t *midi_in;
	snd_rawmidi_t *midi_out;
	lo_server osc_server;
	lo_address osc_send_addr;
	int lo_fd;
	unsigned char midi_buf[4096];
	size_t midi_buf_pos = 0;

	/* Long options */
	const struct option longopts[] = {
		{"help",               0, NULL,               'h'},
		{"device",             1, NULL,               'd'},
		{0, 0, NULL, 0}
	};

	/* Short options */
	while ((c = getopt_long(argc, argv, "hd:",
	                        longopts, NULL)) != -1) {

		switch (c) {

			case 'h' :
			show_help(argv[0]);
			return 0;

			case 'd':
			dev = strdup(optarg);
			break;

			case 0 :
			break;

			default :
			return 1;

		}

	}

	if ( dev == NULL ) {

		/* FIXME: Enumerate and select device */
		dev = strdup("hw:1");

		printf("Found MIDI device: %s\n", dev);

	}

	r = snd_rawmidi_open(&midi_in, &midi_out, dev, SND_RAWMIDI_SYNC);
	if ( r ) {
		fprintf(stderr, "Couldn't open MIDI device: %i\n", r);
		return 1;
	}

	/* Set non-blocking mode for input (read) stream only */
	snd_rawmidi_nonblock(midi_in, 1);

	/* Create OSC server and destination */
	osc_server = lo_server_new("7771", error_callback);
	lo_fd = lo_server_get_socket_fd(osc_server);
	osc_send_addr = lo_address_new(NULL, "7770");

	init_button(&buttons[0], "A", 36);
	init_button(&buttons[1], "B", 37);
	init_button(&buttons[2], "C", 38);
	init_button(&buttons[3], "D", 39);
	init_button(&buttons[4], "E", 32);
	init_button(&buttons[5], "F", 33);
	init_button(&buttons[6], "G", 34);
	init_button(&buttons[7], "H", 35);
	init_button(&buttons[8], "I", 28);
	init_button(&buttons[9], "J", 29);
	init_button(&buttons[10], "K", 30);
	init_button(&buttons[11], "L", 31);
	init_button(&buttons[12], "M", 24);
	init_button(&buttons[13], "N", 25);
	init_button(&buttons[14], "O", 26);
	init_button(&buttons[15], "P", 27);
	init_button_full(&buttons[16], "LAYER", 12, 12, 16, 20);
	init_button_full(&buttons[17], "SHIFT", 15, 15, 19, 23);
	for ( i=0; i<n_buttons; i++ ) {
		add_button_methods(&buttons[i], osc_server);
	}

	init_encoder(&encoders[0], 1, 0, 52);
	init_encoder(&encoders[1], 2, 1, 53);
	init_encoder(&encoders[2], 3, 2, 54);
	init_encoder(&encoders[3], 4, 3, 55);
	init_encoder_noled(&encoders[4], 5, 20, 13);
	init_encoder_noled(&encoders[5], 6, 21, 14);
	for ( i=0; i<n_encoders; i++ ) {
		add_encoder_methods(&encoders[i], osc_server);
	}

	init_fader(&faders[0], 1, 16);
	init_fader(&faders[1], 2, 17);
	init_fader(&faders[2], 3, 18);
	init_fader(&faders[3], 4, 19);
	for ( i=0; i<n_faders; i++ ) {
		add_faderpot_methods(&faders[i], osc_server);
	}

	init_potentiometer(&potentiometers[0], 1, 4, 48);
	init_potentiometer(&potentiometers[1], 2, 5, 49);
	init_potentiometer(&potentiometers[2], 3, 6, 50);
	init_potentiometer(&potentiometers[3], 4, 7, 51);
	init_potentiometer(&potentiometers[4], 5, 8, 44);
	init_potentiometer(&potentiometers[5], 6, 9, 45);
	init_potentiometer(&potentiometers[6], 7, 10, 46);
	init_potentiometer(&potentiometers[7], 8, 11, 47);
	init_potentiometer(&potentiometers[8], 9, 12, 40);
	init_potentiometer(&potentiometers[9], 10, 13, 41);
	init_potentiometer(&potentiometers[10], 11, 14, 42);
	init_potentiometer(&potentiometers[11], 12, 15, 43);
	for ( i=0; i<n_potentiometers; i++ ) {
		add_faderpot_methods(&potentiometers[i], osc_server);
	}

	do {

		struct pollfd pfds[16];
		int nfds;
		int r;

		pfds[0].fd = lo_fd;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;

		/* Add MIDI fds */
		nfds = snd_rawmidi_poll_descriptors(midi_in, &pfds[1], 15);

		r = poll(pfds, 1+nfds, 1000);
		if ( r < 0 ) {
			fprintf(stderr, "poll() failed: %s\n", strerror(errno));
		} else {

			unsigned short revents;

			if ( hup_err(pfds, 1+nfds) ) {
				fprintf(stderr, "Error!\n");
				break;
			}

			if ( pfds[0].revents & POLLIN ) {
				lo_server_recv_noblock(osc_server, 0);
			}

			snd_rawmidi_poll_descriptors_revents(midi_in,
			                                     &pfds[1], nfds,
			                                     &revents);

			if ( (revents & POLLIN) && (midi_buf_pos < 2048) ) {

				ssize_t r;

				r = snd_rawmidi_read(midi_in,
				                     &midi_buf[midi_buf_pos],
				                     4096-midi_buf_pos);
				if ( r < 0 ) {
					fprintf(stderr, "MIDI read failed\n");
				} else {
					size_t total_proc = 0;
					size_t p;
					midi_buf_pos += r;
					do {
						p = process_midi(midi_buf+total_proc,
						                 midi_buf_pos-total_proc,
						                 osc_send_addr);
						total_proc += p;
					} while ( p > 0 );
					memmove(midi_buf, midi_buf+total_proc,
					        4096-total_proc);
					midi_buf_pos -= total_proc;
				}
			}
		}

	} while ( 1 );

	snd_rawmidi_drain(midi_in);
	snd_rawmidi_close(midi_in);

	snd_rawmidi_drain(midi_out);
	snd_rawmidi_close(midi_out);

	return 0;
}
