#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>

#include "net.h"
#include "protocol.h"
#include "config.h"
#include "persist.h"

#define MAX_CLIENTS 16

typedef struct {
	int fds[MAX_CLIENTS];
	int count;
	pthread_mutex_t mtx;
} clients_t;

typedef struct {
	simulation_config_t cfg;
	int cfg_set;

	sim_mode_t mode;
	int done;

	msg_summary_cell_t *summary_cells;

	clients_t clients;

	pthread_t sim_tid;
	pthread_t accept_tid;
	int listen_fd;
} server_t;

static int write_full(int fd, const void *buf, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	size_t off = 0;

	while (off < len) {
		ssize_t n = send(fd, p + off, len - off, MSG_NOSIGNAL);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (n == 0)
			return -1;
		off += (size_t)n;
	}
	return 0;
}

static int read_full(int fd, void *buf, size_t len)
{
	uint8_t *p = (uint8_t *)buf;
	size_t off = 0;

	while (off < len) {
		ssize_t n = recv(fd, p + off, len - off, 0);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (n == 0)
			return 0;
		off += (size_t)n;
	}
	return 1;
}

static void clients_add(clients_t *c, int fd)
{
	pthread_mutex_lock(&c->mtx);
	if (c->count < MAX_CLIENTS) {
		c->fds[c->count++] = fd;
	} else {
		close(fd);
	}
	pthread_mutex_unlock(&c->mtx);
}

static void clients_remove_at(clients_t *c, int idx)
{
	close(c->fds[idx]);
	for (int i = idx; i < c->count - 1; i++)
		c->fds[i] = c->fds[i + 1];
	c->count--;
}

static void broadcast(server_t *s, msg_type_t type, const void *payload, uint32_t size)
{
	msg_header_t hdr;
	hdr.type = type;
	hdr.size = size;

	pthread_mutex_lock(&s->clients.mtx);
	for (int i = 0; i < s->clients.count; ) {
		int fd = s->clients.fds[i];
		if (write_full(fd, &hdr, sizeof(hdr)) != 0 ||
			(size > 0 && write_full(fd, payload, size) != 0)) {
			clients_remove_at(&s->clients, i);
			continue;
		}
		i++;
	}
	pthread_mutex_unlock(&s->clients.mtx);
}

static int is_odd_positive(int v)
{
	return (v > 0) && (v % 2 == 1);
}

static int validate_cfg(const simulation_config_t *cfg)
{
	if (!is_odd_positive(cfg->world_width) || !is_odd_positive(cfg->world_height))
		return 0;
	if (cfg->replications == 0 || cfg->max_steps == 0)
		return 0;

	double sum = cfg->probs.p_up + cfg->probs.p_down + cfg->probs.p_left + cfg->probs.p_right;
	if (sum < 0.999 || sum > 1.001)
		return 0;

	if (cfg->probs.p_up < 0 || cfg->probs.p_down < 0 || cfg->probs.p_left < 0 || cfg->probs.p_right < 0)
		return 0;

	return 1;
}

static double rnd01(void)
{
	return (double)rand() / ((double)RAND_MAX + 1.0);
}

static void wrap_xy(const simulation_config_t *cfg, int *x, int *y)
{
	int min_x = -(cfg->world_width / 2);
	int max_x = +(cfg->world_width / 2);
	int min_y = -(cfg->world_height / 2);
	int max_y = +(cfg->world_height / 2);

	if (*x < min_x) *x = max_x;
	if (*x > max_x) *x = min_x;
	if (*y < min_y) *y = max_y;
	if (*y > max_y) *y = min_y;
}

static void step_one(const simulation_config_t *cfg, int *x, int *y)
{
	double r = rnd01();
	double a = cfg->probs.p_up;
	double b = a + cfg->probs.p_down;
	double c = b + cfg->probs.p_left;

	if (r < a)
		(*y)++;
	else if (r < b)
		(*y)--;
	else if (r < c)
		(*x)--;
	else
		(*x)++;

	wrap_xy(cfg, x, y);
}

static int idx_of(const simulation_config_t *cfg, int x, int y)
{
	int ox = cfg->world_width / 2;
	int oy = cfg->world_height / 2;
	int ix = x + ox;
	int iy = (oy - y);
	return iy * cfg->world_width + ix;
}

static void compute_summary(server_t *s)
{
	const simulation_config_t *cfg = &s->cfg;
	int w = cfg->world_width;
	int h = cfg->world_height;
	int min_x = -(w / 2);
	int max_x = +(w / 2);
	int min_y = -(h / 2);
	int max_y = +(h / 2);

	uint64_t *hits = calloc((size_t)(w * h), sizeof(uint64_t));
	uint64_t *steps_sum = calloc((size_t)(w * h), sizeof(uint64_t));

	for (uint32_t rep = 1; rep <= cfg->replications; rep++) {
		for (int y = min_y; y <= max_y; y++) {
			for (int x = min_x; x <= max_x; x++) {
				if (x == 0 && y == 0) {
					continue;
				}

				int cx = x;
				int cy = y;
				uint32_t steps = 0;
				int hit = 0;

				for (steps = 1; steps <= cfg->max_steps; steps++) {
					step_one(cfg, &cx, &cy);
					if (cx == 0 && cy == 0) {
						hit = 1;
						break;
					}
				}

				int id = idx_of(cfg, x, y);
				if (hit) {
					hits[id]++;
					steps_sum[id] += steps;
				}
			}
		}

		printf("[SERVER] replication %u / %u done\n", rep, cfg->replications);
	}

	s->summary_cells = calloc((size_t)(w * h), sizeof(msg_summary_cell_t));

	for (int y = min_y; y <= max_y; y++) {
		for (int x = min_x; x <= max_x; x++) {
			int id = idx_of(cfg, x, y);

			if (x == 0 && y == 0) {
				s->summary_cells[id].avg_steps = 0.0;
				s->summary_cells[id].probability = 1.0;
				continue;
			}

			double p = (double)hits[id] / (double)cfg->replications;
			double avg = 0.0;
			if (hits[id] > 0)
				avg = (double)steps_sum[id] / (double)hits[id];

			s->summary_cells[id].avg_steps = avg;
			s->summary_cells[id].probability = p;
		}
	}

	free(hits);
	free(steps_sum);
}

static void stream_interactive(server_t *s)
{
	const simulation_config_t *cfg = &s->cfg;

	int start_x = 1;
  int start_y = 0;


	for (uint32_t rep = 1; rep <= cfg->replications; rep++) {
		int x = start_x;
		int y = start_y;

		for (uint32_t step = 1; step <= cfg->max_steps; step++) {
			step_one(cfg, &x, &y);

			msg_interactive_step_t msg;
			msg.x = x;
			msg.y = y;
			msg.step = step;
			msg.replication = rep;
			msg.total_replications = cfg->replications;

			broadcast(s, MSG_INTERACTIVE_STEP, &msg, (uint32_t)sizeof(msg));

			sleep(1);

			if (x == 0 && y == 0)
				break;

			if (s->mode != SIM_MODE_INTERACTIVE)
				break;
		}

		if (s->mode != SIM_MODE_INTERACTIVE)
			break;
	}
}

static void *simulation_thread(void *arg)
{
	server_t *s = (server_t *)arg;

	while (!s->cfg_set)
		sleep(1);

	printf("[SERVER] config received: %dx%d R=%u K=%u\n",
		s->cfg.world_width, s->cfg.world_height,
		s->cfg.replications, s->cfg.max_steps);

	if (s->mode == SIM_MODE_INTERACTIVE)
		stream_interactive(s);

	printf("[SERVER] computing summary...\n");
	compute_summary(s);
	printf("[SERVER] summary ready\n");

  save_simulation(s->cfg.output_file, &s->cfg, s->summary_cells);
  printf("[SERVER] results saved to %s\n", s->cfg.output_file);


	uint32_t total = (uint32_t)(s->cfg.world_width * s->cfg.world_height);
	uint32_t bytes = total * (uint32_t)sizeof(msg_summary_cell_t);

	broadcast(s, MSG_SUMMARY_DATA, s->summary_cells, bytes);

	s->done = 1;
	return NULL;
}

static void send_current_mode(server_t *s, int fd)
{
	msg_header_t hdr;
	hdr.type = MSG_MODE_CHANGE;
	hdr.size = sizeof(sim_mode_t);

	sim_mode_t m = s->mode;

	write_full(fd, &hdr, sizeof(hdr));
	write_full(fd, &m, sizeof(m));
}

static void send_summary_if_ready(server_t *s, int fd)
{
	if (!s->done || s->summary_cells == NULL)
		return;

	uint32_t total = (uint32_t)(s->cfg.world_width * s->cfg.world_height);
	uint32_t bytes = total * (uint32_t)sizeof(msg_summary_cell_t);

	msg_header_t hdr;
	hdr.type = MSG_SUMMARY_DATA;
	hdr.size = bytes;

	write_full(fd, &hdr, sizeof(hdr));
	write_full(fd, s->summary_cells, bytes);
}

static void *accept_thread(void *arg)
{
	server_t *s = (server_t *)arg;

	while (1) {
		int fd = net_accept(s->listen_fd);
		if (fd < 0)
			continue;

		printf("[SERVER] client connected\n");

		if (!s->cfg_set) {
			msg_header_t hdr;
			int r = read_full(fd, &hdr, sizeof(hdr));
			if (r != 1 || hdr.type != MSG_CONFIG || hdr.size != sizeof(simulation_config_t)) {
				close(fd);
				continue;
			}

			simulation_config_t cfg;
			r = read_full(fd, &cfg, sizeof(cfg));
			if (r != 1 || !validate_cfg(&cfg)) {
				close(fd);
				continue;
			}

			s->cfg = cfg;
			s->cfg_set = 1;
			s->mode = cfg.mode;

			clients_add(&s->clients, fd);
			send_current_mode(s, fd);
		} else {
			clients_add(&s->clients, fd);
			send_current_mode(s, fd);
			send_summary_if_ready(s, fd);
		}
	}

	return NULL;
}

int main(void)
{
	setbuf(stdout, NULL);
	srand((unsigned)time(NULL) ^ (unsigned)getpid());

	server_t s;
	memset(&s, 0, sizeof(s));
	pthread_mutex_init(&s.clients.mtx, NULL);

	s.listen_fd = net_listen_unix("/tmp/test.sock");
	if (s.listen_fd < 0)
		return 1;

	printf("[SERVER] listening on /tmp/test.sock\n");

	pthread_create(&s.sim_tid, NULL, simulation_thread, &s);
	pthread_create(&s.accept_tid, NULL, accept_thread, &s);

	pthread_join(s.sim_tid, NULL);

	printf("[SERVER] simulation finished, server exiting\n");

	return 0;
}

