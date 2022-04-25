/*
 * File: ReliableSocket.cpp
 *
 * Reliable data transport (RDT) library implementation.
 *
 * Author(s): Karston Kelly, Russell Gokemeijer
 *
 */

// C++ library includes
#include <iostream>
#include <string.h>
#include <chrono>

// OS specific includes
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ReliableSocket.h"
#include "rdt_time.h"

using std::cerr;
using std::cout;
/*
 * NOTE: Function header comments shouldn't go in this file: they should be put
 * in the ReliableSocket header file.
 */

ReliableSocket::ReliableSocket() {
	this->sequence_number = 0;
	this->expected_sequence_number = 0;
	this->estimated_rtt = 100;
	this->dev_rtt = 10;

	this->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (this->sock_fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	this->state = INIT;
}


void ReliableSocket::accept_connection(int port_num) {
	if (this->state != INIT) {
		cerr << "Cannot call accept on used socket\n";
		exit(EXIT_FAILURE);
	}
	
	// Bind specified port num using our local IPv4 address.
	// This allows remote hosts to connect to a specific port.
	struct sockaddr_in addr; 
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_num);
	addr.sin_addr.s_addr = INADDR_ANY;
	if ( bind(this->sock_fd, (struct sockaddr*)&addr, sizeof(addr)) ) {
		perror("bind");
	}

	// Wait for a segment to come from a remote host
	char segment[MAX_SEG_SIZE];
	memset(segment, 0, MAX_SEG_SIZE);

	struct sockaddr_in fromaddr;
	unsigned int addrlen = sizeof(fromaddr);
	// Will always eventually receive an initial CONN message
	int recv_count = recvfrom(this->sock_fd, segment, MAX_SEG_SIZE, 0, 
								(struct sockaddr*)&fromaddr, &addrlen);		
	if (recv_count < 0) {
		perror("accept recvfrom");
		exit(EXIT_FAILURE);
	}

	/*
	 * UDP isn't connection-oriented, but calling connect here allows us to
	 * remember the remote host (stored in fromaddr).
	 * This means we can then use send and recv instead of the more complex
	 * sendto and recvfrom.
	 */
	if (connect(this->sock_fd, (struct sockaddr*)&fromaddr, addrlen)) {
		perror("accept connect");
		exit(EXIT_FAILURE);
	}

	// Check that segment was the right type of message, namely a RDT_CONN
	// message to indicate that the remote host wants to start a new
	// connection with us.
	RDTHeader* hdr = (RDTHeader*)segment;
	if (hdr->type != RDT_CONN) {
		cerr << "ERROR: Didn't get the expected RDT_CONN type.\n";
		exit(EXIT_FAILURE);
	}

	// Handshaking protocol to make sure that
	// both sides are correctly connected (e.g. what happens if the RDT_CONN
	// message from the other end gets lost?)
	// Note that this function is called by the connection receiver/listener.


	// Send an Ack indicating that we are good to go.
	// Let the sender know which socket we have allocated to them

	hdr->ack_number = htonl(0);
	hdr->sequence_number = htonl(0);
	hdr->type = RDT_CONN;
	int attempts = 0;
	while(this->state != ESTABLISHED){
		if (attempts > 10){
			cerr << "Maximum attempts reached";
			exit(1);
		}
		attempts += 1;
		if (send(this->sock_fd, segment, sizeof(RDTHeader), 0) < 0) {
			perror("ERROR: Did not properly send ACK");
		}
		
		char received_segment[MAX_SEG_SIZE];
		memset(received_segment, 0, MAX_SEG_SIZE);

		this->set_timeout_length(this->estimated_rtt*1.5);
		if (recv(this->sock_fd, received_segment, MAX_SEG_SIZE, 0) != EWOULDBLOCK){
			attempts = 0;
			RDTHeader* rec_hdr = (RDTHeader*)received_segment;
			if(rec_hdr->type == RDT_ACK){
				this->state = ESTABLISHED;
				this->expected_sequence_number += 1;
				
				// Make it so no other recv calls for the receiver timeout
				this->set_timeout_length(0);
				cerr << "INFO: Connection ESTABLISHED\n";
				break;
			}
		}
	}
}

void ReliableSocket::connect_to_remote(char *hostname, int port_num) {
	if (this->state != INIT) {
		cerr << "Cannot call connect_to_remote on used socket\n";
		return;
	}
	
	// set up IPv4 address info with given hostname and port number
	struct sockaddr_in addr; 
	addr.sin_family = AF_INET; 	// use IPv4
	addr.sin_addr.s_addr = inet_addr(hostname);
	addr.sin_port = htons(port_num); 

	/*
	 * UDP isn't connection-oriented, but calling connect here allows us to
	 * remember the remote host (stored in fromaddr).
	 * This means we can then use send and recv instead of the more complex
	 * sendto and recvfrom.
	 */
	if(connect(this->sock_fd, (struct sockaddr*)&addr, sizeof(addr))) {
		perror("connect");
	}

	// Send an RDT_CONN message to remote host to initiate an RDT connection.
	char segment[sizeof(RDTHeader)];
	memset(segment, 0, sizeof(RDTHeader));	
	RDTHeader* hdr = (RDTHeader*)segment;

	hdr->ack_number = htonl(0);
	hdr->sequence_number = htonl(0);
	hdr->type = RDT_CONN;
	
	// Handshaking protocol for the connection setup.
	// Note that this function is called by the connection initiator.
	// Loop checks if 20 failed connection attempts occured, exits if they did
	// Otherwise establishes reliable connection with remote host.

	int attempts = 0;
	while (this->state != ESTABLISHED){
		if (attempts >= 10){
			perror("Failed to connect to host.\n");
			exit(1);
		}
		attempts += 1;
		if (send(this->sock_fd, segment, sizeof(RDTHeader), 0) < 0) {
			perror("conn1 send");
		}

		// Start timer, wait for ACK
		// Also checks that the response from the receiver is the correct ACK 
		this->set_timeout_length(this->estimated_rtt*1.5);
		char received_segment[MAX_SEG_SIZE];
		memset(received_segment, 0, MAX_SEG_SIZE);
		if(recv(this->sock_fd, received_segment, MAX_SEG_SIZE, 0) != EWOULDBLOCK){
			RDTHeader* rec_hdr = (RDTHeader*)received_segment;
			memset(received_segment, 0, MAX_SEG_SIZE);
			if(rec_hdr->type == RDT_CONN){
				this->state = ESTABLISHED;
				this->sequence_number += 1;
				cerr << "INFO: Connection ESTABLISHED\n";
				hdr->type = RDT_ACK;
				if (send(this->sock_fd, segment, sizeof(RDTHeader), 0) < 0) {
					perror("End of handshake fail");
				}
				break;
			}
		}
	}
}


// We did not modify this function in any way.
uint32_t ReliableSocket::get_estimated_rtt() {
	return this->estimated_rtt;
}

// We did not modify this function in any way.
void ReliableSocket::set_timeout_length(uint32_t timeout_length_ms) {
	cerr << "INFO: Setting timeout to " << timeout_length_ms << " ms\n";
	struct timeval timeout;
	msec_to_timeval(timeout_length_ms, &timeout);

	if (setsockopt(this->sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
					sizeof(struct timeval)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
}

void ReliableSocket::send_data(const void *data, int length) {
	if (this->state != ESTABLISHED) {
		cerr << "INFO: Cannot send: Connection not established.\n";
		return;
	}

 	// Create the segment, which contains a header followed by the data.
	char segment[MAX_SEG_SIZE];
	memset(segment, 0, MAX_SEG_SIZE);

	// Fill in the header
	RDTHeader *hdr = (RDTHeader*)segment;
	
	hdr->sequence_number = htonl(this->sequence_number);
	hdr->ack_number = htonl(0);
	hdr->type = RDT_DATA;
	// Copy the user-supplied data to the spot right past the 
	// 	header (i.e. hdr+1).
	memcpy(hdr+1, data, length);
	this->set_timeout_length(std::min((int)(this->estimated_rtt * 1.5), 500));
	int attempts = 0;
	while(true){
		if (attempts > 10){
			cerr << "Maximum data send attempt exceeded exiting\n";
			exit(EXIT_FAILURE);
		}
		cerr << "Sending pakcage " << ntohl(hdr->sequence_number) << "\n";
		if (send(this->sock_fd, segment, sizeof(RDTHeader)+length, 0) < 0) {
			perror("send_data send");
			exit(EXIT_FAILURE);
		}
		auto start_time = std::chrono::system_clock::now();
		char received_segment[MAX_SEG_SIZE];
		memset(received_segment, 0, MAX_SEG_SIZE);
		if (recv(this->sock_fd, received_segment, MAX_SEG_SIZE, 0) > 0){
			attempts += 1;
			RDTHeader* rec_hdr = (RDTHeader*)received_segment;
			if((rec_hdr->type == RDT_ACK) && (this->sequence_number == rec_hdr->ack_number)){
				auto end_time = std::chrono::system_clock::now();
				std::chrono::duration<double> sample_rtt = end_time - start_time;
				this->estimated_rtt = std::min((int)(.875 * this->estimated_rtt + .125 * sample_rtt.count()), 500);
				cerr << "This is the new rtt: " << this->estimated_rtt << "\n";
				cerr << "INFO: Data Packet " << rec_hdr->ack_number << " Sent and Received\n";
				this->sequence_number += 1;
				return;
			}
			
			// IF the third message of the handshake is lost we may
			// receive an additional RDT_CONN message and that is handled
			// here and then the ack and the sending data is resent
			else if(rec_hdr->type == RDT_CONN){
				char send_segment[sizeof(RDTHeader)];
				memset(send_segment, 0, sizeof(RDTHeader));	
				RDTHeader* send_hdr = (RDTHeader*)send_segment;
				send_hdr->ack_number = htonl(0);
				send_hdr->sequence_number = htonl(0);
				send_hdr->type = RDT_ACK;
				if (send(this->sock_fd, send_segment, sizeof(RDTHeader), 0) < 0) {
					perror("Error sending ack in response to CONN\n");
				}
			}
		}
		else{
			cerr << "Oh no a timeout thats ok ill change estimated r_tt\n";
			this->estimated_rtt = (int)(1.2*this->estimated_rtt);
			this->set_timeout_length(std::min((int)(this->estimated_rtt * 1.5), 500));
		}
	}
	// Made code that waits for an acknowledgment of the data you just sent, and keeps
	// resending until that ack comes.
	// Utilizes the set_timeout_length function to make sure you timeout after
	// a certain amount of waiting (so you can try sending again).
}

// In receiver.cpp
int ReliableSocket::receive_data(char buffer[MAX_DATA_SIZE]) {
	if (this->state != ESTABLISHED) {
		cerr << "INFO: Cannot receive: Connection not established.\n";
		return 0;
	}

	// Sent back an acknowledment that we
	// received some data, but first we made sure that what we
	// received was the type we wanted (RDT_DATA) and had the right sequence
	// number.

	//If the data received is good
	while (true){
		char received_segment[MAX_SEG_SIZE];
		memset(received_segment, 0, MAX_SEG_SIZE);
		int recv_count = recv(this->sock_fd, received_segment, MAX_SEG_SIZE, 0);
		if (recv_count < 0) {
			perror("receive_data recv");
			exit(EXIT_FAILURE);
		}
		RDTHeader* hdr = (RDTHeader*)received_segment;
		// If the package we received is the next set of data
		// cerr << "Received a packet with seq: " << ntohl(hdr->sequence_number) << " Expected: " << this->expected_sequence_number << "\n";
		if(hdr->type == RDT_DATA && this->expected_sequence_number == ntohl(hdr->sequence_number)){
			// cerr << "Received a Data packet\n";
			char send_segment[sizeof(RDTHeader)];
			memset(send_segment, 0, sizeof(RDTHeader));
			RDTHeader* send_hdr = (RDTHeader*)send_segment;
			send_hdr->ack_number = htonl(hdr->sequence_number);
			send_hdr->type = RDT_ACK;
			// cerr << "Sending ack: " << ntohl(hdr->sequence_number) << "\n";
			if (send(this->sock_fd, send_segment, sizeof(RDTHeader), 0) < 0) {
				perror("Error sending ack in response to data received");
			}

			cerr << "INFO: Received segment. " 
			<< "seq_num = "<< ntohl(hdr->sequence_number) << ", "
			//<< "ack_num = "<< ntohl(hdr->ack_number) << ", "
			<< ", type = " << hdr->type << "\n";

			void *data = (void*)(received_segment + sizeof(RDTHeader));
			this->expected_sequence_number += 1;
			int recv_data_size = recv_count - sizeof(RDTHeader);
			memcpy(buffer, data, recv_data_size);
			return recv_data_size;
		}
		else if(hdr->type == RDT_DATA && (this->expected_sequence_number > ntohl(hdr->sequence_number)) && (ntohl(hdr->sequence_number) > 0)){
			char send_segment[sizeof(RDTHeader)];
			memset(send_segment, 0, sizeof(RDTHeader));
			RDTHeader* send_hdr = (RDTHeader*)send_segment;
			send_hdr->ack_number = ntohl(hdr->sequence_number);
			send_hdr->type = RDT_ACK;
			cerr << "Sending out of order ack: " << ntohl(hdr->sequence_number) << " I expect: " << this->expected_sequence_number << "\n";
			if (send(this->sock_fd, send_segment, sizeof(RDTHeader), 0) < 0) {
				perror("Error sending ack for repeat received segment");
			}
		}
		else if(hdr->type == RDT_CLOSE){
			// cerr << "Received a packet but it was the wrong type\n";
			return 0;
		}
	}
}

void ReliableSocket::close_connection() {
	// Construct a RDT_CLOSE message to indicate to the remote host that we
	// want to end this connection.
	char segment[sizeof(RDTHeader)];
	memset(segment, 0, sizeof(RDTHeader));	
	RDTHeader* hdr = (RDTHeader*)segment;

	hdr->sequence_number = htonl(0);
	hdr->ack_number = htonl(0);
	hdr->type = RDT_CLOSE;

	// Reliably closies the connection to make sure both sides know that the
	// connection has been closed.

	this->set_timeout_length(this->estimated_rtt*1.5);
	int timeouts = 0;
	while(true){
		if (timeouts > 5){
			break;
		}
		timeouts+=1;
		if (send(this->sock_fd, segment, sizeof(RDTHeader), 0) < 0) {
			perror("close send");
		}
		char received_segment[MAX_SEG_SIZE];
		memset(received_segment, 0, MAX_SEG_SIZE);
		int recv_count = recv(this->sock_fd, received_segment, MAX_SEG_SIZE, 0);
		//Catch timeout
		if (recv_count == -1){
			this->estimated_rtt = (int)(1.2*this->estimated_rtt);
			this->set_timeout_length(this->estimated_rtt*1.5);
		}
		else if (recv_count < 0) {
			perror("receive_data recv");
			exit(EXIT_FAILURE);
		}
		else{
			RDTHeader* rec_hdr = (RDTHeader*)received_segment;
			if(rec_hdr->type == RDT_CLOSE){
				hdr->type = RDT_ACK;
				cerr << "Received a close packet exiting\n";
				if (send(this->sock_fd, segment, sizeof(RDTHeader), 0) < 0){
					perror("error sending ack in response to close");
					exit(1);
				}
				this->state = CLOSED;
				break;
			}
			else if(rec_hdr->type == RDT_ACK){
				cerr << "Recieved ACK packet exiting\n";
				this->state = CLOSED;
				break;
			}
			else if(rec_hdr->type == RDT_DATA){
				char send_segment[sizeof(RDTHeader)];
				memset(send_segment, 0, sizeof(RDTHeader));
				RDTHeader* send_hdr = (RDTHeader*)send_segment;
				send_hdr->ack_number = ntohl(rec_hdr->sequence_number);
				send_hdr->type = RDT_ACK;
				cerr << "Sending last repeat ack: " << ntohl(rec_hdr->sequence_number) << " for received repeat packet\n";
				if (send(this->sock_fd, send_segment, sizeof(RDTHeader), 0) < 0) {
					perror("Error sending ack for repeat received segment");
				}
			}
		}
	}
	if (close(this->sock_fd) < 0) {
		perror("close_connection close");
	}
}
