
#include "../../include/storage.hpp"

void tunnels_handler::add(unsigned char *target,
						  unsigned char *from,
						  struct init_tunnel init) {
	struct tunnel *one;
	int cmp1, cmp2;
	bool is_snd, r;

	is_snd = (init.type == TCP_SND) ? true : false;
	assert(target && from);
	mute.lock();

	for (auto &p : structs::tunnels) {
		cmp1 = memcmp(p->target, target, HASHSIZE);
		cmp2 = memcmp(p->from,   from,   HASHSIZE);

		if (cmp1 != 0 || cmp2 != 0) {
			continue;
		}

		r = ((is_snd && p->send.work)
			|| (!is_snd && p->recv.work));

		if (r) {
			mute.unlock();
			return;
		}

		if (is_snd) {
			p->send.init(init, p);
			mute.unlock();
			return;
		}

		p->recv.init(init, p);
		mute.unlock();
		return;
	}

	assert(one = new tunnel());

	one->time = system_clock::now();
	HASHCPY(one->target, target);
	HASHCPY(one->from, from);
	one->length = 0;

	structs::tunnels.push_back(one);

	if (is_snd) {
		one->send.init(init, one);
		mute.unlock();
		return;
	}

	one->recv.init(init, one);
	mute.unlock();
}

bool tunnels_handler::find_ports(unsigned char *target,
								 unsigned char *from,
								 pair<size_t, size_t> &d) {
	bool status = false;
	int cmp1, cmp2;

	assert(target && from);
	mute.lock();

	for (auto &p : structs::tunnels) {
		cmp1 = memcmp(p->target, target, HASHSIZE);
		cmp2 = memcmp(p->from,   from,   HASHSIZE);

		if (p->status < TUNNEL_3 && cmp1 == 0
			&& cmp2 == 0) {
			status = true;
			d = p->ports();
			break;
		}
	}

	mute.unlock();
	return status;
}

bool tunnels_handler::find(unsigned char *target,
						   unsigned char *from,
						   enum tcp_role role,
						   struct tunnel **buff) {
	bool status = false;
	int cmp1, cmp2;

	assert(target && from);
	mute.lock();

	*buff = nullptr;

	for (auto &p : structs::tunnels) {
		cmp1 = memcmp(p->target, target, HASHSIZE);
		cmp2 = memcmp(p->from, from, HASHSIZE);

		if (cmp1 == 0 && cmp2 == 0
			&& p->role() == role) {
			status = true;
			*buff = p;
			break;
		}
	}

	mute.unlock();
	return status;
}

size_t tunnels_handler::free_port(void) {
	pair<size_t, size_t> t_ports;
	vector<size_t>::iterator it;
	vector<size_t> list;
	size_t port = 49160;

	mute.lock();

	for (auto &p : structs::tunnels) {
		t_ports = p->ports();
		list.push_back(t_ports.first);
		list.push_back(t_ports.second);
	}

	for (;; port++) {
		it = std::find(list.begin(), list.end(), port);
		assert(port != 0);

		if (it == list.end()
			&& this->sys_freeport(port)) {
			break;
		}
	}

	mute.unlock();
	return port;
}

bool tunnels_handler::sys_freeport(size_t port) {
	int tsock = socket(AF_INET, SOCK_STREAM, 0);
	socklen_t sz = sizeof(struct sockaddr_in);
	bool status;

	st.sddr.sin_port = htons(port);

	status = bind(tsock, st.ptr, sz) == 0;
	CLOSE_SOCKET(tsock);

	return status;
}

bool tunnels_handler::is_freeport(size_t port) {
	pair<size_t, size_t> t_ports;
	vector<size_t>::iterator it;
	vector<size_t> list;

	mute.lock();

	for (auto &p : structs::tunnels) {
		t_ports = p->ports();
		list.push_back(t_ports.first);
		list.push_back(t_ports.second);
	}

	mute.unlock();

	it = std::find(list.begin(), list.end(), port);
	return it == list.end();
}

void tunnels_handler::check(void) {
	using structs::tunnels;

	size_t size;

	mute.lock();

	if ((size = tunnels.size()) == 0) {
		mute.unlock();
		return;
	}

	for (size_t i = 0; i < size; i++) {
		if (!tunnels[i]->work()) {
			tunnels[i]->mute.unlock();
			delete tunnels[i];
			tunnels.erase(tunnels.begin() + i);
		}
	}

	mute.unlock();
}