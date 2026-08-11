#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[]) {

	if (argc < 2) {
		fprintf(stderr, "Usage: %s --encode|--decode\n", argv[0]);
		return 1;
	}

	pid_t pid = fork();

	if (pid < 0) {
		perror("Fork failed!\n");
		return 1;
	}

	if (pid == 0) {
		char exe_path[1024];
		char target_bin[1024];

		ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if (len == -1) {
			perror("Failed to read /proc/self/exe\n");
			exit(1);
		}
		exe_path[len] = '\0';

		char *bin_dir = dirname(exe_path);

		if (strcmp(argv[1], "--encode") == 0) {
			snprintf(target_bin, sizeof(target_bin), "%s/encoder", bin_dir);
			// Branch logic goes here
		}
		else if (strcmp(argv[1], "--decode") == 0) {
			snprintf(target_bin, sizeof(target_bin), "%s/decoder", bin_dir);
			// And here too
		}
		else {
			fprintf(stderr, "Unknown command: %s\n", argv[1]);
			exit(1);
		}

		execv(target_bin, &argv[1]);

		fprintf(stderr, "%s: exec failed\n", target_bin);
		exit(1);
	}

	int status;
	waitpid(pid, &status, 0);

	return 0;
}

