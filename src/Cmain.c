#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>

#include "net.h"
#include "protocol.h"
#include "config.h"

typedef struct {
	int sock_fd;

	int world_width;
	int world_height;

	display_type_t display_type;

	msg_summary_cell_t *summary;
	size_t summary_size;
} client_ctx_t;

/* ================= CONFIG ================= */

int load_config_from_args(int argc, char *argv[], simulation_config_t *cfg)
{
	if (argc != 9)
		return -1;

	cfg->world_width = atoi(argv[1]);
	cfg->world_height = atoi(argv[2]);
	cfg->replications = (uint32_t)atoi(argv[3]);
	cfg->max_steps = (uint32_t)atoi(argv[4]);

	cfg->probs.p_up = atof(argv[5]);
	cfg->probs.p_down = atof(argv[6]);
	cfg->probs.p_left = atof(argv[7]);
	cfg->probs.p_right = atof(argv[8]);

	cfg->mode = SIM_MODE_SUMMARY;
	strcpy(cfg->output_file, "result.txt");

	double sum =
		cfg->probs.p_up +
		cfg->probs.p_down +
		cfg->probs.p_left +
		cfg->probs.p_right;

	if (sum < 0.999 || sum > 1.001)
		return -1;

	return 0;
}

/* ================= DISPLAY ================= */

void display_summary(const client_ctx_t *ctx)
{
	printf("\033[H\033[J");

	printf(ctx->display_type == DISPLAY_AVG ?
		"SUMMARY – AVG STEPS\n\n" :
		"SUMMARY – PROBABILITY\n\n");

	for (int y = 0; y < ctx->world_height; y++) {
		for (int x = 0; x < ctx->world_width; x++) {
			msg_summary_cell_t *c =
				&ctx->summary[y * ctx->world_width + x];

			if (ctx->display_type == DISPLAY_AVG)
				printf("%6.1f ", c->avg_steps);
			else
				printf("%6.2f ", c->probability);
		}
		printf("\n");
	}

	printf("\n[a] avg  [p] prob  [q] quit\n");
}

void display_interactive(const msg_interactive_step_t *s)
{
	printf("\033[H\033[J");
	printf("INTERACTIVE MODE\n\n");
	printf("Replication %u / %u\n",
		s->replication,
		s->total_replications);
	printf("Step: %u\n", s->step);
	printf("Position: (%d,%d)\n", s->x, s->y);
}

/* ================= RECEIVE THREAD ================= */

void *recv_thread(void *arg)
{
	client_ctx_t *ctx = arg;

	while (1) {
		msg_header_t hdr;

		if (recv(ctx->sock_fd, &hdr, sizeof(hdr), 0) <= 0)
			break;

		if (hdr.type == MSG_INTERACTIVE_STEP) {
			msg_interactive_step_t step;
			recv(ctx->sock_fd, &step, sizeof(step), 0);
			display_interactive(&step);
		}

		if (hdr.type == MSG_SUMMARY_DATA) {
			free(ctx->summary);

			ctx->summary_size = hdr.size;
			ctx->summary = malloc(hdr.size);

			if (!ctx->summary)
				break;

			recv(ctx->sock_fd, ctx->summary, hdr.size, 0);
			display_summary(ctx);
		}
	}

	return NULL;
}

/* ================= MAIN ================= */

int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);

	simulation_config_t cfg;

	if (load_config_from_args(argc, argv, &cfg) != 0) {
		printf("Usage:\n");
		printf("./client WIDTH HEIGHT R K p_up p_down p_left p_right\n");
		return 1;
	}

	client_ctx_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.display_type = DISPLAY_AVG;
	ctx.world_width = cfg.world_width;
	ctx.world_height = cfg.world_height;

	printf("Client starting...\n");

	ctx.sock_fd = net_connect_unix("/tmp/test.sock");
	if (ctx.sock_fd < 0) {
		printf("Connection failed\n");
		return 1;
	}

	printf("Connected to server\n");

	msg_header_t hdr;
	hdr.type = MSG_CONFIG;
	hdr.size = sizeof(cfg);

	send(ctx.sock_fd, &hdr, sizeof(hdr), 0);
	send(ctx.sock_fd, &cfg, sizeof(cfg), 0);

	pthread_t tid;
	pthread_create(&tid, NULL, recv_thread, &ctx);

	while (1) {
		int c = getchar();

		if (c == 'a') {
			ctx.display_type = DISPLAY_AVG;
			if (ctx.summary)
				display_summary(&ctx);
		}

		if (c == 'p') {
			ctx.display_type = DISPLAY_PROB;
			if (ctx.summary)
				display_summary(&ctx);
		}

		if (c == 'q')
			break;
	}

	free(ctx.summary);
	close(ctx.sock_fd);
	return 0;
}

