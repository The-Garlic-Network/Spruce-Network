
#include "../include/network.hpp"

void udp_network::start(void) {
	sock = new_socket(SOCK_DGRAM, TIMEOUT);

	if (sock == 0) {
		cout << "[E]: Can't create new socket.\n";
		return;
	}

	if (!structs::father.status) {
		storage::nodes.temporary_father();
	}

	work = true;

	thr1 = thread(&udp_network::send_thread, this);
	thr2 = thread(&udp_network::recv_thread, this);

	if (!thr1.joinable() || !thr2.joinable()) {
		work = false;
	}
}
/*********************************************************/
void udp_network::processing(struct sockaddr_in sddr,
							 pack msg) {
	THREAD_START();
if ("73636d0377047b908d310572a51dc3e60fe0b410ca90991cf77fe1d0a8a27f29" == bin2hex(HASHSIZE, msg.hash())) {
	cout << "Received: " << msg.type() << endl;
	//msg.debug();
}
	{
		using storage::clients;

		bool is_ping = msg.type() == USER_REQ_PING
					|| msg.type() == NODE_REQ_PING;

		switch (structs::role) {
		case UDP_NODE:
			clients.reg(msg.hash(), ipport_get(sddr),
						msg.role(), is_ping);

			handler::node(msg, sddr);
			break;

		case UDP_USER:
			handler::user(msg, sddr);
			break;

		default:
			if (msg.type() != RES_ROLE) {
				break;
			}

			handler::role(msg, sddr);
		}
	}

	THREAD_END();
}
/*********************************************************/
void udp_network::send_thread(void) {
	using structs::father;
	using storage::tasks;
	using structs::role;

	time_point<system_clock> time, ping;
	struct sockaddr_in sddr;
	struct udp_task task;
	bool sendt;
	pack msg;

	ping = system_clock::now();

	while ((time = system_clock::now()), work) {
		/**
		*	Checking the paternal node.
		*/
		if (time - father.info.ping > 15s && father.status) {  cout << "Fuck father.\n";
			storage::father.no_father();
			tasks.renew();
			continue;
		}
		/**
		*	If we just connected to the network and we have
		*	not yet been assigned a role.
		*/
		if (role == UDP_NONE && !tasks.exists(REQ_ROLE)) {
			msg.tmp(REQ_ROLE);
			tasks.add(msg, sddr_get(father.info.ipp),
					  father.info.hash);                      cout << "New REQ_ROLE.\n";
		}

		sendt = tasks.first(task);
		/**
		*	We receive a package for sending if we have
		*	already received a role in the network.
		*
		*	If there are no messages to send, create a
		*	PING packet to the father node.
		*/
		if (!sendt && role != UDP_NONE
			&& system_clock::now() - ping >= 2s) {
			msg.tmp((role == UDP_USER) ? USER_REQ_PING
									   : NODE_REQ_PING);
			sddr = sddr_get(father.info.ipp);
			task = msg.to_task(father.info.hash, &sddr);
			ping = system_clock::now();
			sendt = true;
		}

		if (sendt) {
			this->send_package(task);
		}
	}
}
/*********************************************************/
void udp_network::send_package(struct udp_task task) {
	struct sockaddr *ptr;
	socklen_t size;

	ptr = reinterpret_cast<struct sockaddr *>(&task.sddr);
	size = sizeof(struct sockaddr_in);
	task.sddr.sin_family = AF_INET;

	int aa = sendto(sock, task.buff, task.len, 0x100, ptr, size);

	if (aa == -1) {
		// Добавить пакет снова в список заданий если это ответ!
		//this_thread::sleep_for(1s);
		cout << "sendto error\n";
	}

	if (task.type == USER_REQ_FIND) {
		cout << "sent USER_REQ_FIND\n";
	}
}
/*********************************************************/
void udp_network::recv_thread(void) {
	socklen_t sz = sizeof(struct sockaddr_in);
	unsigned char buff[MAX_UDPSZ];
	size_t rs, p_sz = MAX_UDPSZ;
	struct sddr_structs cln;
	pack msg;

	assert(bind(sock, recv.ptr, sz) == 0);

	while (work) {
		/**
		*	We receive a packet from a network participant,
		*	check it and create a stream for processing.
		*/
		rs = recvfrom(sock, buff, p_sz, 0x100, cln.ptr, &sz);
		msg = pack(buff, rs);

		if (rs < UDP_PACK || !msg.is_correct()) {
			cout << "Incorrect from (" << rs << "): " << ipport_get(cln.sddr).ip << endl;
			memset(buff, 0x00, p_sz);
			continue;
		}
		/**
		*	A new thread is created so as not to delay the
		*	message receiving cycle.
		*/
		thread(&udp_network::processing, this, cln.sddr,
			   msg).detach();
		memset(buff, 0x00, p_sz);
	}
}

