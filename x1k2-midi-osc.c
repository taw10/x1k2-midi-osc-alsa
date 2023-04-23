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
	snd_rawmidi_drain(midi_out);
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
	snd_rawmidi_drain(midi_out);
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


static void add_led(lo_server osc_server, snd_rawmidi_t *midi_out,
                    int led, int red, int orange, int green)
{
	char tmp[256];
	struct led_callback_data *cb;

	cb = malloc(sizeof(struct led_callback_data));
	if ( cb == NULL ) return;

	cb->midi_out = midi_out;
	cb->red = red;
	cb->orange = orange;
	cb->green = green;

	snprintf(tmp, 255, "/x1k2/leds/%i", led);
	lo_server_add_method(osc_server, tmp, "s", led_handler, cb);
}


static void handle_note_off(int note, int vel, lo_address osc_send_addr)
{
	printf("note off %i, vel %i\n", note, vel);
}


static void handle_note(int note, int vel, lo_address osc_send_addr)
{
	printf("note %i, vel %i\n", note, vel);
	switch ( note ) {

		case 26:
		lo_send(osc_send_addr, "/starlet/selection/clear", "");
		break;

		case 32:
		lo_send(osc_send_addr, "/starlet/selection/mhLL", "");
		break;

		case 33:
		lo_send(osc_send_addr, "/starlet/selection/mhL", "");
		break;

		case 34:
		lo_send(osc_send_addr, "/starlet/selection/mhR", "");
		break;

		case 35:
		lo_send(osc_send_addr, "/starlet/selection/mhRR", "");
		break;

	}
}


static void handle_cc(int cc, int val, lo_address osc_send_addr)
{
	printf("CC %i = %i\n", cc, val);
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


static int flip_position(int i)
{
	int x = i % 4;
	int y = i / 4;
	y = 7-y;
	return 1+y*4+x;
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

	osc_server = lo_server_new("7771", error_callback);
	lo_fd = lo_server_get_socket_fd(osc_server);
	osc_send_addr = lo_address_new(NULL, "7770");

	for ( i=0; i<32; i++ ) {
		add_led(osc_server, midi_out, flip_position(i),
		        i+24, i+60, i+96);
	}
	add_led(osc_server, midi_out, 101, 12, 16, 20);
	add_led(osc_server, midi_out, 102, 15, 19, 23);

	do {

		struct pollfd pfds[16];
		int nfds;
		int r;

		pfds[0].fd = lo_fd;
		pfds[0].events = POLLIN | POLLOUT;
		pfds[0].revents = 0;

		/* Add MIDI fds */
		nfds = snd_rawmidi_poll_descriptors(midi_in, &pfds[1], 15);

		r = poll(pfds, 1+nfds, 1000);
		if ( r < 0 ) {
			fprintf(stderr, "poll() failed: %s\n", strerror(errno));
		} else {

			unsigned short revents;

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
					printf("%li new bytes\n", r);
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
