/*
 * The program simulates a Token Ring LAN by forking off a process
 * for each LAN node, that communicate via shared memory, instead
 * of network cables. To keep the implementation simple, it jiggles
 * out bytes instead of bits.
 *
 * It keeps a count of packets sent and received for each node.
 */
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include "tokenRing.h"

/**
 * This function is the body of a child process emulating a node.
 */
void
token_node(control, num)
	struct TokenRingData *control;
	int num;
{
	int rcv_state = TOKEN_FLAG, not_done = 1, sending = 0, len = -1, sender;
	unsigned char byte;
	/*
	 * If this is node #0, start the ball rolling by creating the
	 * token.
	 */

	if (num == 0) {
		send_byte(control, num, UNUSED_TOKEN);

#ifdef DEBUG
		fprintf(stderr, "Created first token at node #%d.\n", num);
#endif
	}

	/*
	 * Loop around processing data, until done.
	 */
	while (not_done) {
		//???


		byte = rcv_byte(control, num);
		/*
		 * Handle the byte, based upon current state.
		 */




		switch (rcv_state) {
		case TOKEN_FLAG:


			//if token is not 1 set to next if 1 set back to token flag
			// all possible things you would do when you get a token
			// used token you call sendpkt
			WAIT_SEM(control, CRIT);
			if ((byte == UNUSED_TOKEN)
				&& (control->shared_ptr->node[num].to_send.token_flag == SENDER)) {
				sender = '1';
			}
			else {
				sender = '0';
			}
			
			SIGNAL_SEM(control, CRIT);
			if (byte == UNUSED_TOKEN) {

				if (sender == '1') {

					control->snd_state = TOKEN_FLAG;

					send_pkt(control, num);	
					rcv_state = TO;

				}
				
				else {

					WAIT_SEM(control, CRIT);

					
					if (control->shared_ptr->node[num].terminate == 1) {
						printf("Exiting child: %d\n", num);
						not_done = 0;

					}

					SIGNAL_SEM(control, CRIT);
					
					send_byte(control, num, byte);
					rcv_state = TOKEN_FLAG;
				}

			}

			else {
				send_byte(control, num, byte);
				rcv_state = TO;
			}

			break;

		case TO:
			// also do sending
			if (sender == '1') {
				send_pkt(control, num);
			}
			else {
				send_byte(control, num, byte);
			}
			rcv_state = FROM;	

			break;

		case FROM:

			if (sender == '1') {
				send_pkt(control, num);
			}
			else {
				send_byte(control, num, byte);
			}
			rcv_state = LEN;

			break;

		case LEN:
			// if length is not 0, go to data

			if (sender == '1') {
				send_pkt(control, num);
				WAIT_SEM(control, CRIT);
				len = control->shared_ptr->node[num].to_send.length;
				SIGNAL_SEM(control, CRIT);
			}
			else {
				send_byte(control, num, byte);
				len = (int) byte;
			}

			sending = 0;

			if (len > 0) {
				rcv_state = DATA;
			}
			else {
				rcv_state = TOKEN_FLAG;
			}

			break;

		case DATA:

			
			if (sending < (len-1)) {
				if (sender == '1') {
					send_pkt(control, num);
				}
				else {
					send_byte(control, num, byte);
				}	
				rcv_state = DATA;
			}
			else {

				if (sender == '1') {
					send_pkt(control, num);

				}
				else {
					send_byte(control, num, byte);
				}

				sender = '0';
				rcv_state = TOKEN_FLAG;

			}

			sending++;
			

			break;
		};

		
		
		
	}

}
/*
*
* to control each individual producer consumer relationship Empty and filled are analogous to available and used
*
*send another free token when received the info that was sent
*
*if you want to send a message, if you get a free token, set it to used token 
*
*in the every other state machine 
*
*in the send packet if you are the from
*
*wait on empty when in a situation where someone will produce a unit of emptiness in the queue
*
*the node that does packet sending cannot go to sleep
*
*after send last byte of data, send free token, thats when tell parent TO_SEND
*
*consume is waiting on cue then signal available
*/
/*
 * This function sends a data packet followed by the token, one byte each
 * time it is called.
 */
void
send_pkt(control, num)
	struct TokenRingData *control;
	int num;
{
	static int sndpos, sndlen, i;


	switch (control->snd_state) {
	case TOKEN_FLAG:
		
		WAIT_SEM(control, CRIT);
		control->shared_ptr->node[num].sent = control->shared_ptr->node[num].sent + 1;
		i = (int) control->shared_ptr->node[num].to_send.to;
		control->shared_ptr->node[i].received++;
		control->shared_ptr->node[num].to_send.token_flag = USED_TOKEN;
		SIGNAL_SEM(control, CRIT);
		
		send_byte(control, num, control->shared_ptr->node[num].to_send.token_flag);
		
		control->snd_state = TO;

		sndpos = 0;

		WAIT_SEM(control, CRIT);
		sndlen = control->shared_ptr->node[num].to_send.length;
		SIGNAL_SEM(control, CRIT);

		break;

	case TO:

		send_byte(control, num, control->shared_ptr->node[num].to_send.to);

		control->snd_state = FROM;

		break;

	case FROM:
		
		send_byte(control, num, control->shared_ptr->node[num].to_send.from);

		control->snd_state = LEN;

		break;

	case LEN:
		
		send_byte(control, num, control->shared_ptr->node[num].to_send.length);

		control->snd_state = DATA;
		break;

	case DATA:	
		if (sndpos < (sndlen-1)) {
		
			send_byte(control, num, control->shared_ptr->node[num].to_send.data[sndpos]);
			
			WAIT_SEM(control, CRIT);
			sndpos++;
			control->snd_state = DATA;

			SIGNAL_SEM(control, CRIT);
			
			break;
		}
		else {	

			WAIT_SEM(control, CRIT);
			control->snd_state = DONE;
			SIGNAL_SEM(control, CRIT);
		}

	case DONE:
		WAIT_SEM(control, CRIT);
		printf("\nData at node: %d is: ", num);
		for (i = 0; i < control->shared_ptr->node[num].to_send.length; i++) {
			printf("%c", control->shared_ptr->node[num].to_send.data[i]);
		}
		printf("\n\n");
		control->shared_ptr->node[num].to_send.token_flag = NOT_SENDER;
		
		control->snd_state = TOKEN_FLAG;
		send_byte(control, num, UNUSED_TOKEN);

		SIGNAL_SEM(control, TO_SEND(num));

		SIGNAL_SEM(control, CRIT);
		break;
	};

	
}

/*
 * Send a byte to the next node on the ring.
 */
void
send_byte(control, num, byte)
	struct TokenRingData *control;
	int num;
	unsigned byte;
{
	WAIT_SEM(control, EMPTY(num));
	control->shared_ptr->node[num].data_xfer = byte;
#ifdef DEBUG
		fprintf(stderr, "Sent byte %c at node #%d.\n", byte, num);
#endif

	SIGNAL_SEM(control, FILLED(num));
}

/*
 * Receive a byte for this node.
 */
unsigned char
rcv_byte(control, num)
	struct TokenRingData *control;
	int num;
{
	unsigned char byte;
	int rcv_from;

	rcv_from = (num + (N_NODES-1))%N_NODES;
	WAIT_SEM(control, FILLED(rcv_from));
	byte = control->shared_ptr->node[rcv_from].data_xfer;

#ifdef DEBUG
		fprintf(stderr, "Received byte %c at node #%d.\n", byte, num);
#endif	
	SIGNAL_SEM(control, EMPTY(rcv_from));

	return byte;
}

