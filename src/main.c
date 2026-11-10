#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/wait.h>
#include <unistd.h>

#define BG_VERSION "1.0.0"

typedef enum { MODE_NONE, MODE_ENCODE, MODE_DECODE } OpMode;
typedef enum { FMT_AUTO, FMT_PNG, FMT_JPG } ImgFormat;

static void print_help(const char *prog) {
    printf("\033[1;36mBlueGachiin \033[0m v%s\n\n", BG_VERSION);
    printf("\033[1mUSAGE:\033[0m\n");
    printf("  %s -e | -d -i <image> [options]\n\n", prog);
    printf("\033[1mCORE MODES (Required):\033[0m\n");
    printf("  -e, --encode             Encode / embed payload into carrier\n");
    printf("  -d, --decode             Decode / extract payload from carrier\n\n");
    printf("\033[1mFORMAT OVERRIDE (Optional - auto-detected by default):\033[0m\n");
    printf("      --png                Force PNG pipeline (Haar DWT)\n");
    printf("      --jpg, --jpeg        Force JPEG pipeline (DCT Matrix)\n\n");
    printf("\033[1mOPTIONS:\033[0m\n");
    printf("  -i, --input <file>       Input carrier image\n");
    printf("  -o, --output <file>      Output target image / payload file\n");
    printf("  -k, --key <passphrase>   Cryptographic PRNG seed key (K_shared)\n");
    printf("  -s, --sig <string>       Raw 64-byte payload text\n");
    printf("  -f, --sig-file <file>    Path to 64-byte binary payload\n");
    printf("  -v, --verbose            Enable verbose telemetry\n");
    printf("  -h, --help               Display this help guide\n\n");
    printf("\033[1mEXAMPLES:\033[0m\n");
    printf("  %s -e -i cover.png -o stego.png -k \"my-seed\" -s \"secret_token_123\"\n", prog);
    printf("  %s -d -i stego.jpg -k \"my-seed\"\n", prog);
}

// Sniff file extension if format flag wasn't explicitly passed
static ImgFormat detect_format_from_path(const char *path) {
    if (!path) return FMT_AUTO;
    const char *dot = strrchr(path, '.');
    if (!dot) return FMT_AUTO;

    if (strcasecmp(dot, ".png") == 0) return FMT_PNG;
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) return FMT_JPG;
    return FMT_AUTO;
}

int main(int argc, char *argv[]) {
    OpMode mode = MODE_NONE;
    ImgFormat fmt = FMT_AUTO;
    char *input_file = NULL;
    char *output_file = NULL;
    char *key = NULL;
    char *sig_str = NULL;
    char *sig_file = NULL;
    bool verbose = false;

    enum { OPT_PNG = 1000, OPT_JPG };

    static struct option long_options[] = {
        {"encode",   no_argument,       0, 'e'},
        {"decode",   no_argument,       0, 'd'},
        {"png",      no_argument,       0, OPT_PNG},
        {"jpg",      no_argument,       0, OPT_JPG},
        {"jpeg",     no_argument,       0, OPT_JPG},
        {"input",    required_argument, 0, 'i'},
        {"output",   required_argument, 0, 'o'},
        {"key",      required_argument, 0, 'k'},
        {"sig",      required_argument, 0, 's'},
        {"sig-file", required_argument, 0, 'f'},
        {"verbose",  no_argument,       0, 'v'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "edi:o:k:s:f:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'e': mode = MODE_ENCODE; break;
            case 'd': mode = MODE_DECODE; break;
            case OPT_PNG: fmt = FMT_PNG; break;
            case OPT_JPG: fmt = FMT_JPG; break;
            case 'i': input_file = optarg; break;
            case 'o': output_file = optarg; break;
            case 'k': key = optarg; break;
            case 's': sig_str = optarg; break;
            case 'f': sig_file = optarg; break;
            case 'v': verbose = true; break;
            case 'h':
                print_help(argv[0]);
                return 0;
            default:
                fprintf(stderr, "Try '%s --help' for usage.\n", argv[0]);
                return 1;
        }
    }

    // 1. Validate fundamental inputs
    if (mode == MODE_NONE) {
        fprintf(stderr, "Error: Specify either -e (--encode) or -d (--decode)\n");
        return 1;
    }
    if (!input_file) {
        fprintf(stderr, "Error: Input file (-i/--input) is required\n");
        return 1;
    }

    // 2. Resolve image type (CLI flag overrides file extension)
    if (fmt == FMT_AUTO) {
        fmt = detect_format_from_path(input_file);
    }
    if (fmt == FMT_AUTO) {
        fprintf(stderr, "Error: Could not determine image type. Pass --png or --jpg explicitly.\n");
        return 1;
    }

    // 3. Resolve target child binary directory
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        perror("readlink /proc/self/exe");
        return 1;
    }
    exe_path[len] = '\0';
    char *bin_dir = dirname(exe_path);

    char target_bin[1024];
    const char *sub_bin = NULL;
    if (mode == MODE_ENCODE) {
        sub_bin = (fmt == FMT_PNG) ? "pe" : "je";
    } else {
        sub_bin = (fmt == FMT_PNG) ? "pd" : "jd";
    }
    snprintf(target_bin, sizeof(target_bin), "%s/%s", bin_dir, sub_bin);

    // 4. Construct clean child argv array to forward down
    char *child_argv[32];
    int c_argc = 0;
    child_argv[c_argc++] = (char *)sub_bin;

    if (input_file)  { child_argv[c_argc++] = "-i"; child_argv[c_argc++] = input_file; }
    if (output_file) { child_argv[c_argc++] = "-o"; child_argv[c_argc++] = output_file; }
    if (key)         { child_argv[c_argc++] = "-k"; child_argv[c_argc++] = key; }
    if (sig_str)     { child_argv[c_argc++] = "-s"; child_argv[c_argc++] = sig_str; }
    if (sig_file)    { child_argv[c_argc++] = "-f"; child_argv[c_argc++] = sig_file; }
    if (verbose)     { child_argv[c_argc++] = "-v"; }
    child_argv[c_argc] = NULL;

    // 5. Fork & dispatch
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (pid == 0) {
        execv(target_bin, child_argv);
        fprintf(stderr, "Failed to exec %s: ", target_bin);
        perror("");
        exit(1);
    }

    // 6. Wait and preserve child exit code
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return 1;
}
