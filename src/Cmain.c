#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdint.h>
#include <errno.h>

#include "net.h"
#include "protocol.h"
#include "config.h"

typedef enum {
	DISPLAY_AVG,
	DISPLAY_PROB
} display_t;

typedef struct {
	int sock_fd;

	int world_width;
	int world_height;

	pthread_mutex_t mtx;

	uint8_t *obstacles;
	int obstacles_ready;

	msg_sum_cell_t *summary;
	int summary_ready;

	display_t display;
} client_ctx_t;

static int read_full(int fd, void *buf, size_t len)
{
	uint8_t *p = buf;
	size_t off = 0;

	while (off < len) {
		ssize_t n = recv(fd, p + off, len - off, 0);
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

static void clear_screen(void)
{
	printf("\033[H\033[J");
}

static void draw_world(client_ctx_t *ctx, int wx, int wy)
{
	int w = ctx->world_width;
	int h = ctx->world_height;

	int ox = w / 2;
	int oy = h / 2;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int gx = x - ox;
			int gy = oy - y;

			if (gx == wx && gy == wy) {
				putchar('@');
				continue;
			}

			if (ctx->obstacles_ready && ctx->obstacles && ctx->obstacles[y * w + x])
				putchar('#');
			else
				putchar('.');
		}
		putchar('\n');
	}
}

static void display_interactive(client_ctx_t *ctx, const msg_int_t *m)
{
	clear_screen();
	printf("=== INTERACTIVE MODE ===\n\n");
	printf("Replication: %u / %u\n", (unsigned)m->replication, (unsigned)m->total_replications);
	printf("Step: %u\n", (unsigned)m->step);
	printf("Position: (%d, %d)\n\n", m->x, m->y);

	pthread_mutex_lock(&ctx->mtx);
	if (ctx->world_width > 0 && ctx->world_height > 0)
		draw_world(ctx, m->x, m->y);
	pthread_mutex_unlock(&ctx->mtx);

	printf("\n(waiting for summary...)\n");
	fflush(stdout);
}

static void display_summary(client_ctx_t *ctx)
{
	int w = ctx->world_width;
	int h = ctx->world_height;

	clear_screen();
	printf("=== SUMMARY (%s) ===\n\n",
		ctx->display == DISPLAY_AVG ? "AVG STEPS" : "PROBABILITY");

	if (!ctx->summary_ready || !ctx->summary || w <= 0 || h <= 0) {
		printf("(summary not ready)\n\n");
		printf("[a] avg   [p] prob   [q] quit\n");
		fflush(stdout);
		return;
	}

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int id = y * w + x;

			if (ctx->obstacles_ready && ctx->obstacles && ctx->obstacles[id]) {
				printf("   ### ");
				continue;
			}

			msg_sum_cell_t *c = &ctx->summary[id];
			if (ctx->display == DISPLAY_AVG)
				printf("%6.2f ", c->avg_steps);
			else
				printf("%6.2f ", c->probability);
		}
		printf("\n");
	}

	printf("\n[a] avg   [p] prob   [q] quit\n");
	fflush(stdout);
}

static void *recv_thread(void *arg)
{
	client_ctx_t *ctx = arg;

	while (1) {
		msg_header_t hdr;

		if (read_full(ctx->sock_fd, &hdr, sizeof(hdr)) != 0)
			break;

		if (hdr.type == MSG_OBSTACLES) {
			uint8_t *buf = malloc(hdr.size);
			if (!buf)
				break;

			if (read_full(ctx->sock_fd, buf, hdr.size) != 0) {
				free(buf);
				break;
			}

			pthread_mutex_lock(&ctx->mtx);
			free(ctx->obstacles);
			ctx->obstacles = buf;
			ctx->obstacles_ready = 1;
			pthread_mutex_unlock(&ctx->mtx);

		} else if (hdr.type == MSG_INTERACTIVE_STEP) {
			msg_int_t msg;

			if (hdr.size != sizeof(msg)) {
				uint8_t *skip = malloc(hdr.size);
				if (!skip)
					break;
				if (read_full(ctx->sock_fd, skip, hdr.size) != 0) {
					free(skip);
					break;
				}
				free(skip);
				continue;
			}

			if (read_full(ctx->sock_fd, &msg, sizeof(msg)) != 0)
				break;

			display_interactive(ctx, &msg);

		} else if (hdr.type == MSG_SUMMARY_DATA) {
			msg_sum_cell_t *buf = malloc(hdr.size);
			if (!buf)
				break;

			if (read_full(ctx->sock_fd, buf, hdr.size) != 0) {
				free(buf);
				break;
			}

			pthread_mutex_lock(&ctx->mtx);
			free(ctx->summary);
			ctx->summary = buf;
			ctx->summary_ready = 1;
			ctx->display = DISPLAY_AVG;
			display_summary(ctx);
			pthread_mutex_unlock(&ctx->mtx);

		} else {
			if (hdr.size > 0) {
				uint8_t *skip = malloc(hdr.size);
				if (!skip)
					break;
				if (read_full(ctx->sock_fd, skip, hdr.size) != 0) {
					free(skip);
					break;
				}
				free(skip);
			}
		}
	}

	return NULL;
}

static void run_client(const config *cfg, const char *sock_path, int send_cfg)
{
	client_ctx_t ctx;

	memset(&ctx, 0, sizeof(ctx));
	pthread_mutex_init(&ctx.mtx, NULL);

	if (cfg) {
		ctx.world_width = cfg->world_width;
		ctx.world_height = cfg->world_height;
	}
	ctx.display = DISPLAY_AVG;

	printf("[CLIENT] connecting to %s...\n", sock_path);

	ctx.sock_fd = net_connect_unix(sock_path);
	if (ctx.sock_fd < 0) {
		printf("[CLIENT] connection failed\n");
		return;
	}

	printf("[CLIENT] connected\n");

	if (send_cfg && cfg) {
		msg_header_t hdr;
		hdr.type = MSG_CONFIG;
		hdr.size = sizeof(*cfg);

		if (write_full(ctx.sock_fd, &hdr, sizeof(hdr)) != 0 ||
		    write_full(ctx.sock_fd, cfg, sizeof(*cfg)) != 0) {
			printf("[CLIENT] failed to send config\n");
			close(ctx.sock_fd);
			return;
		}
	}

	pthread_t tid;
	pthread_create(&tid, NULL, recv_thread, &ctx);

	while (1) {
		int ch = getchar();

		pthread_mutex_lock(&ctx.mtx);
		int ready = ctx.summary_ready;
		pthread_mutex_unlock(&ctx.mtx);

		if (ready) {
			if (ch == 'a') {
				pthread_mutex_lock(&ctx.mtx);
				ctx.display = DISPLAY_AVG;
				display_summary(&ctx);
				pthread_mutex_unlock(&ctx.mtx);
			} else if (ch == 'p') {
				pthread_mutex_lock(&ctx.mtx);
				ctx.display = DISPLAY_PROB;
				display_summary(&ctx);
				pthread_mutex_unlock(&ctx.mtx);
			}
		}

		if (ch == 'q')
			break;
	}

	shutdown(ctx.sock_fd, SHUT_RDWR);
	close(ctx.sock_fd);

	pthread_join(tid, NULL);
	pthread_mutex_destroy(&ctx.mtx);

	free(ctx.obstacles);
	free(ctx.summary);
}

static void server_sock_from_pid(char *out, size_t n, pid_t pid)
{
	snprintf(out, n, "/tmp/sim_%d.sock", (int)pid);
}

static pid_t start_server(char *sock_out, size_t n)
{
	pid_t pid = fork();
	if (pid == 0) {
		execl("./server", "./server", NULL);
		_exit(1);
	}
	if (pid < 0)
		return -1;

	server_sock_from_pid(sock_out, n, pid);
	sleep(1);
	return pid;
}

static int ask_int(const char *prompt)
{
	char line[128];
	int v = 0;

	for (;;) {
		printf("%s", prompt);
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin))
			exit(1);

		if (sscanf(line, "%d", &v) == 1)
			return v;

		printf("Invalid input, try again.\n");
	}
}

static double ask_double(const char *prompt)
{
	char line[128];
	double v = 0.0;

	for (;;) {
		printf("%s", prompt);
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin))
			exit(1);

		if (sscanf(line, "%lf", &v) == 1)
			return v;

		printf("Invalid input, try again.\n");
	}
}

static void ask_str(const char *prompt, char *out, size_t n)
{
	char line[512];

	for (;;) {
		printf("%s", prompt);
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin))
			exit(1);

		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[len - 1] = '\0';
			len--;
		}

		if (len == 0) {
			printf("Empty input, try again.\n");
			continue;
		}

		if (len >= n) {
			printf("Too long (max %zu chars), try again.\n", n - 1);
			continue;
		}

		snprintf(out, n, "%s", line);
		return;
	}
}

static int parse_width_height_from_file(const char *path, int *w_out, int *h_out)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;

	char key[64];
	int w = 0, h = 0;

	for (int i = 0; i < 32; i++) {
		if (fscanf(f, "%63s", key) != 1)
			break;

		if (strcmp(key, "WIDTH") == 0) {
			if (fscanf(f, "%d", &w) != 1)
				break;
		} else if (strcmp(key, "HEIGHT") == 0) {
			if (fscanf(f, "%d", &h) != 1)
				break;
		} else {
			char skipline[512];
			fgets(skipline, sizeof(skipline), f);
		}

		if (w > 0 && h > 0)
			break;
	}

	fclose(f);

	if (w <= 0 || h <= 0)
		return -1;

	*w_out = w;
	*h_out = h;
	return 0;
}

static void menu_header(void)
{
	clear_screen();
	printf("====================================\n");
	printf("   RANDOM WALK SIMULATION (POS)\n");
	printf("====================================\n\n");
}

static void menu_new(void)
{
	menu_header();

	config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.start_type = SIM_NEW;

	for (;;) {
		cfg.world_width = ask_int("World width (odd): ");
		cfg.world_height = ask_int("World height (odd): ");
		if (cfg.world_width > 0 && cfg.world_height > 0 &&
		    (cfg.world_width % 2 == 1) && (cfg.world_height % 2 == 1))
			break;
		printf("World must be positive odd x odd.\n");
	}

	cfg.replications = (uint32_t)ask_int("Replications: ");
	cfg.max_steps = (uint32_t)ask_int("Max steps K: ");

	cfg.probs.p_up = ask_double("p_up: ");
	cfg.probs.p_down = ask_double("p_down: ");
	cfg.probs.p_left = ask_double("p_left: ");
	cfg.probs.p_right = ask_double("p_right: ");

	cfg.world_type = (world_type_t)ask_int("World type (1=empty, 2=obstacles): ");
	if (cfg.world_type != WORLD_OBSTACLES)
		cfg.world_type = WORLD_EMPTY;

	if (cfg.world_type == WORLD_OBSTACLES)
		cfg.obstacle_density = ask_double("Obstacle density (0.0 - 0.6): ");
	else
		cfg.obstacle_density = 0.0;

	cfg.mode = (sim_mode_t)ask_int("Mode (1=interactive, 2=summary): ");
	if (cfg.mode != SIM_MODE_SUMMARY)
		cfg.mode = SIM_MODE_INTERACTIVE;

	ask_str("Output file: ", cfg.output_file, sizeof(cfg.output_file));

	char sock[108];
	pid_t pid = start_server(sock, sizeof(sock));
	if (pid < 0) {
		printf("[CLIENT] failed to start server\n");
		return;
	}

	printf("\n[CLIENT] server started (pid=%d)\n", (int)pid);
	printf("[CLIENT] connecting automatically: %s\n\n", sock);

	run_client(&cfg, sock, 1);
}

static void menu_load(void)
{
	menu_header();

	config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.start_type = SIM_LOAD;
	cfg.mode = SIM_MODE_SUMMARY;

	ask_str("Input file: ", cfg.input_file, sizeof(cfg.input_file));

	int w = 0, h = 0;
	if (parse_width_height_from_file(cfg.input_file, &w, &h) != 0) {
		printf("\n[CLIENT] failed to parse WIDTH/HEIGHT from file\n");
		printf("Press Enter...\n");
		getchar();
		return;
	}

	cfg.world_width = w;
	cfg.world_height = h;

	cfg.replications = (uint32_t)ask_int("Replications (ignored for load, keep any): ");
	ask_str("Output file: ", cfg.output_file, sizeof(cfg.output_file));

	char sock[108];
	pid_t pid = start_server(sock, sizeof(sock));
	if (pid < 0) {
		printf("[CLIENT] failed to start server\n");
		return;
	}

	printf("\n[CLIENT] server started (pid=%d)\n", (int)pid);
	printf("[CLIENT] connecting automatically: %s\n\n", sock);

	run_client(&cfg, sock, 1);
}

static void menu_connect(void)
{
	menu_header();

	int pid = ask_int("Enter server PID to connect: ");

	char sock[108];
	server_sock_from_pid(sock, sizeof(sock), (pid_t)pid);

	config dummy;
	memset(&dummy, 0, sizeof(dummy));

	dummy.world_width = ask_int("World width: ");
	dummy.world_height = ask_int("World height: ");

	printf("\n[CLIENT] connecting: %s\n\n", sock);
	run_client(&dummy, sock, 0);
}

int main(void)
{
	setbuf(stdout, NULL);

	for (;;) {
		menu_header();
		printf("1) New simulation\n");
		printf("2) Load simulation from file\n");
		printf("3) Connect to running server (by PID)\n");
		printf("4) Quit\n\n");

		int choice = ask_int("Choice: ");

		if (choice == 1)
			menu_new();
		else if (choice == 2)
			menu_load();
		else if (choice == 3)
			menu_connect();
		else if (choice == 4)
			return 0;
		else
			printf("Invalid choice\n");
	}
}

