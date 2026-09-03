#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  if (argc < 3) {
    fprintf(stderr, "Usage: ./bg --encode|--decode --png|--jpg\n");
    return 1;
  }

  pid_t pid = fork();

  if (pid < 0) {
    perror("Fork failed!");
    return 1;
  }

  if (pid == 0) {
    char exe_path[1024];
    char target_bin[1024];

    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
      perror("Failed to read /proc/self/exe");
      exit(1);
    }
    exe_path[len] = '\0';

    char *bin_dir = dirname(exe_path);

    if (strcmp(argv[1], "--encode") == 0) {
      if (strcmp(argv[2], "--png") == 0) {
        snprintf(target_bin, sizeof(target_bin), "%s/pe", bin_dir);
      }

      else if (strcmp(argv[2], "--jpg") == 0) {
        snprintf(target_bin, sizeof(target_bin), "%s/je", bin_dir);
      }
    } else if (strcmp(argv[1], "--decode") == 0) {
        if (strcmp(argv[2], "--png") == 0) {
          snprintf(target_bin, sizeof(target_bin), "%s/pd", bin_dir);
        } else if (strcmp(argv[2], "--jpg") == 0) {
          snprintf(target_bin, sizeof(target_bin), "%s/jd", bin_dir);
        }
    } else {
      fprintf(stderr, "Unknown command: %s\n", argv[1]);
      exit(1);
    }

    for (int i = 1; i < (argc - 1); i++) {
      printf("%s\t", argv[i]);
    }

    execv(target_bin, &argv[2]);

    fprintf(stderr, "%s: exec failed\n", target_bin);
    exit(1);
  }

  int status;
  waitpid(pid, &status, 0);

  return 0;
}
