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
	config cfg;
	int cfg_set;

	int done;
	msg_sum_cell_t *summary_cells;

	uint8_t *obstacles;

	clients_t clients;

	pthread_t sim_tid;
	pthread_t accept_tid;
	int listen_fd;
	char sock_path[108];
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
	if (c->count < MAX_CLIENTS)
		c->fds[c->count++] = fd;
	else
		close(fd);
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

static int validate_cfg(const config *cfg)
{
	if (cfg->start_type == SIM_NEW) {
		if (!is_odd_positive(cfg->world_width) || !is_odd_positive(cfg->world_height))
			return 0;
		if (cfg->replications == 0 || cfg->max_steps == 0)
			return 0;

		double sum = cfg->probs.p_up + cfg->probs.p_down + cfg->probs.p_left + cfg->probs.p_right;
		if (sum < 0.999 || sum > 1.001)
			return 0;
		if (cfg->probs.p_up < 0 || cfg->probs.p_down < 0 || cfg->probs.p_left < 0 || cfg->probs.p_right < 0)
			return 0;

		if (cfg->world_type == WORLD_OBSTACLES) {
			if (cfg->obstacle_density < 0.0 || cfg->obstacle_density > 0.6)
				return 0;
		}
		return 1;
	}

	if (cfg->start_type == SIM_LOAD) {
		if (cfg->input_file[0] == '\0')
			return 0;
		return 1;
	}

	return 0;
}

static double rnd01(void)
{
	return (double)rand() / ((double)RAND_MAX + 1.0);
}

static void wrap_xy(const config *cfg, int *x, int *y)
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

static int idx_of(const config *cfg, int x, int y)
{
	int ox = cfg->world_width / 2;
	int oy = cfg->world_height / 2;
	int ix = x + ox;
	int iy = (oy - y);
	return iy * cfg->world_width + ix;
}

static int is_obstacle(const server_t *s, int x, int y)
{
	if (s->cfg.world_type != WORLD_OBSTACLES)
		return 0;
	if (!s->obstacles)
		return 0;
	return s->obstacles[idx_of(&s->cfg, x, y)] != 0;
}

static void step_one(server_t *s, int *x, int *y)
{
	double r = rnd01();
	double a = s->cfg.probs.p_up;
	double b = a + s->cfg.probs.p_down;
	double c = b + s->cfg.probs.p_left;

	int nx = *x;
	int ny = *y;

	if (r < a)
		ny++;
	else if (r < b)
		ny--;
	else if (r < c)
		nx--;
	else
		nx++;

	wrap_xy(&s->cfg, &nx, &ny);

	if (!is_obstacle(s, nx, ny)) {
		*x = nx;
		*y = ny;
	}
}

static int validate_obstacles(server_t *s)
{
	int w = s->cfg.world_width;
	int h = s->cfg.world_height;
	int total = w * h;

	uint8_t *vis = calloc((size_t)total, 1);
	int *qx = malloc((size_t)total * sizeof(int));
	int *qy = malloc((size_t)total * sizeof(int));
	if (!vis || !qx || !qy) {
		free(vis);
		free(qx);
		free(qy);
		return 0;
	}

	if (is_obstacle(s, 0, 0)) {
		free(vis);
		free(qx);
		free(qy);
		return 0;
	}

	int head = 0, tail = 0;
	qx[tail] = 0;
	qy[tail] = 0;
	tail++;

	vis[idx_of(&s->cfg, 0, 0)] = 1;

	while (head < tail) {
		int x = qx[head];
		int y = qy[head];
		head++;

		static const int dx[4] = {1, -1, 0, 0};
		static const int dy[4] = {0, 0, 1, -1};

		for (int i = 0; i < 4; i++) {
			int nx = x + dx[i];
			int ny = y + dy[i];

			wrap_xy(&s->cfg, &nx, &ny);

			int id = idx_of(&s->cfg, nx, ny);
			if (vis[id])
				continue;
			if (is_obstacle(s, nx, ny))
				continue;

			vis[id] = 1;
			qx[tail] = nx;
			qy[tail] = ny;
			tail++;
		}
	}

	for (int i = 0; i < total; i++) {
		if (s->obstacles[i] == 0 && vis[i] == 0) {
			free(vis);
			free(qx);
			free(qy);
			return 0;
		}
	}

	free(vis);
	free(qx);
	free(qy);
	return 1;
}

static void generate_obstacles(server_t *s)
{
	int w = s->cfg.world_width;
	int h = s->cfg.world_height;
	int total = w * h;

	free(s->obstacles);
	s->obstacles = calloc((size_t)total, 1);
	if (!s->obstacles)
		return;

	int min_x = -(w / 2);
	int max_x = +(w / 2);
	int min_y = -(h / 2);
	int max_y = +(h / 2);

	for (int y = min_y; y <= max_y; y++) {
		for (int x = min_x; x <= max_x; x++) {
			if (x == 0 && y == 0)
				continue;
			if (rnd01() < s->cfg.obstacle_density)
				s->obstacles[idx_of(&s->cfg, x, y)] = 1;
		}
	}
}

static void ensure_obstacles(server_t *s)
{
	if (s->cfg.world_type != WORLD_OBSTACLES) {
		free(s->obstacles);
		s->obstacles = NULL;
		return;
	}

	for (int tries = 0; tries < 200; tries++) {
		generate_obstacles(s);
		if (s->obstacles && validate_obstacles(s)) {
			printf("[SERVER] obstacles ready (density=%.2f)\n", s->cfg.obstacle_density);
			return;
		}
	}

	printf("[SERVER] obstacle generation failed -> fallback to empty world\n");
	s->cfg.world_type = WORLD_EMPTY;
	free(s->obstacles);
	s->obstacles = NULL;
}

static void compute_summary(server_t *s)
{
	const config *cfg = &s->cfg;
	int w = cfg->world_width;
	int h = cfg->world_height;
	int min_x = -(w / 2);
	int max_x = +(w / 2);
	int min_y = -(h / 2);
	int max_y = +(h / 2);

	uint64_t *hits = calloc((size_t)(w * h), sizeof(uint64_t));
	uint64_t *steps_sum = calloc((size_t)(w * h), sizeof(uint64_t));
	if (!hits || !steps_sum) {
		free(hits);
		free(steps_sum);
		return;
	}

	for (uint32_t rep = 1; rep <= cfg->replications; rep++) {
		for (int y = min_y; y <= max_y; y++) {
			for (int x = min_x; x <= max_x; x++) {
				if (x == 0 && y == 0)
					continue;

				if (is_obstacle(s, x, y))
					continue;

				int cx = x;
				int cy = y;
				uint32_t steps;
				int hit = 0;

				for (steps = 1; steps <= cfg->max_steps; steps++) {
					step_one(s, &cx, &cy);
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
		printf("[SERVER] replication %u / %u done\n", (unsigned)rep, (unsigned)cfg->replications);
	}

	free(s->summary_cells);
	s->summary_cells = calloc((size_t)(w * h), sizeof(msg_sum_cell_t));
	if (!s->summary_cells) {
		free(hits);
		free(steps_sum);
		return;
	}

	for (int y = min_y; y <= max_y; y++) {
		for (int x = min_x; x <= max_x; x++) {
			int id = idx_of(cfg, x, y);

			if (x == 0 && y == 0) {
				s->summary_cells[id].avg_steps = 0.0;
				s->summary_cells[id].probability = 1.0;
				continue;
			}

			if (is_obstacle(s, x, y)) {
				s->summary_cells[id].avg_steps = 0.0;
				s->summary_cells[id].probability = 0.0;
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

static void send_obstacles_to_fd(server_t *s, int fd)
{
	uint32_t size = (uint32_t)(s->cfg.world_width * s->cfg.world_height);

	msg_header_t hdr;
	hdr.type = MSG_OBSTACLES;
	hdr.size = size;

	uint8_t *tmp = NULL;

	if (s->cfg.world_type == WORLD_OBSTACLES && s->obstacles) {
		tmp = s->obstacles;
	} else {
		tmp = calloc((size_t)size, 1);
		if (!tmp)
			return;
	}

	write_full(fd, &hdr, sizeof(hdr));
	write_full(fd, tmp, size);

	if (tmp != s->obstacles)
		free(tmp);
}

static void broadcast_obstacles(server_t *s)
{
	uint32_t size = (uint32_t)(s->cfg.world_width * s->cfg.world_height);

	if (s->cfg.world_type == WORLD_OBSTACLES && s->obstacles) {
		broadcast(s, MSG_OBSTACLES, s->obstacles, size);
	} else {
		uint8_t *zero = calloc((size_t)size, 1);
		if (!zero)
			return;
		broadcast(s, MSG_OBSTACLES, zero, size);
		free(zero);
	}
}

static void send_summary_to_fd(server_t *s, int fd)
{
	if (!s->done || !s->summary_cells)
		return;

	uint32_t total = (uint32_t)(s->cfg.world_width * s->cfg.world_height);
	uint32_t bytes = total * (uint32_t)sizeof(msg_sum_cell_t);

	msg_header_t hdr;
	hdr.type = MSG_SUMMARY_DATA;
	hdr.size = bytes;

	write_full(fd, &hdr, sizeof(hdr));
	write_full(fd, s->summary_cells, bytes);
}

static void run_interactive(server_t *s)
{
	for (uint32_t rep = 1; rep <= s->cfg.replications; rep++) {
		int x = 0;
		int y = 0;

		for (uint32_t step = 1; step <= s->cfg.max_steps; step++) {
			step_one(s, &x, &y);

			msg_int_t m;
			m.x = x;
			m.y = y;
			m.step = step;
			m.replication = rep;
			m.total_replications = s->cfg.replications;

			broadcast(s, MSG_INTERACTIVE_STEP, &m, (uint32_t)sizeof(m));
			sleep(1);

			if (x == 0 && y == 0)
				break;
		}
	}
}

static void *simulation_thread(void *arg)
{
	server_t *s = (server_t *)arg;

	while (!s->cfg_set)
		sleep(1);

	free(s->summary_cells);
	s->summary_cells = NULL;
	free(s->obstacles);
	s->obstacles = NULL;
	s->done = 0;

	if (s->cfg.start_type == SIM_LOAD) {
		printf("[SERVER] loading simulation from %s\n", s->cfg.input_file);

		if (load_simulation(s->cfg.input_file, &s->cfg, &s->obstacles, &s->summary_cells) != 0) {
			printf("[SERVER] load failed\n");
			s->done = 1;
			return NULL;
		}

		printf("[SERVER] loaded: %dx%d R=%u K=%u type=%d\n",
			s->cfg.world_width, s->cfg.world_height,
			(unsigned)s->cfg.replications, (unsigned)s->cfg.max_steps,
			(int)s->cfg.world_type);

		broadcast_obstacles(s);

		if (s->summary_cells) {
			uint32_t total = (uint32_t)(s->cfg.world_width * s->cfg.world_height);
			uint32_t bytes = total * (uint32_t)sizeof(msg_sum_cell_t);
			broadcast(s, MSG_SUMMARY_DATA, s->summary_cells, bytes);
		}

		s->done = 1;

		if (s->cfg.output_file[0] != '\0' && s->summary_cells) {
			save_simulation(s->cfg.output_file, &s->cfg, s->obstacles, s->summary_cells);
			printf("[SERVER] results saved to %s\n", s->cfg.output_file);
		}

		return NULL;
	}

	printf("[SERVER] new simulation: %dx%d R=%u K=%u type=%d\n",
		s->cfg.world_width, s->cfg.world_height,
		(unsigned)s->cfg.replications, (unsigned)s->cfg.max_steps,
		(int)s->cfg.world_type);

	ensure_obstacles(s);
	broadcast_obstacles(s);

	if (s->cfg.mode == SIM_MODE_INTERACTIVE) {
		printf("[SERVER] interactive start\n");
		run_interactive(s);
		printf("[SERVER] interactive done\n");
	}

	printf("[SERVER] computing summary...\n");
	compute_summary(s);

	if (s->summary_cells && s->cfg.output_file[0] != '\0') {
		save_simulation(s->cfg.output_file, &s->cfg, s->obstacles, s->summary_cells);
		printf("[SERVER] results saved to %s\n", s->cfg.output_file);
	}

	if (s->summary_cells) {
		uint32_t total = (uint32_t)(s->cfg.world_width * s->cfg.world_height);
		uint32_t bytes = total * (uint32_t)sizeof(msg_sum_cell_t);
		broadcast(s, MSG_SUMMARY_DATA, s->summary_cells, bytes);
	}

	s->done = 1;
	printf("[SERVER] summary ready\n");
	return NULL;
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
			if (r != 1 || hdr.type != MSG_CONFIG || hdr.size != sizeof(config)) {
				close(fd);
				continue;
			}

			config cfg;
			r = read_full(fd, &cfg, sizeof(cfg));
			if (r != 1 || !validate_cfg(&cfg)) {
				close(fd);
				continue;
			}

			s->cfg = cfg;
			s->cfg_set = 1;

			clients_add(&s->clients, fd);

			send_obstacles_to_fd(s, fd);
			send_summary_to_fd(s, fd);
		} else {
			clients_add(&s->clients, fd);
			send_obstacles_to_fd(s, fd);
			send_summary_to_fd(s, fd);
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

	snprintf(s.sock_path, sizeof(s.sock_path), "/tmp/sim_%d.sock", getpid());

	s.listen_fd = net_listen_unix(s.sock_path);
	if (s.listen_fd < 0)
		return 1;

	printf("[SERVER] listening on %s\n", s.sock_path);

	pthread_create(&s.sim_tid, NULL, simulation_thread, &s);
	pthread_create(&s.accept_tid, NULL, accept_thread, &s);

	pthread_join(s.sim_tid, NULL);

	printf("[SERVER] simulation finished, server stays up for connects\n");

	while (1)
		sleep(1);
}

