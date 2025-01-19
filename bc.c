#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define SMALL_USAGE "usage: [file][-o out file][-S][-c][-v][-C bc1 path][-a as path][-l ld path][-h]\n"
#define USAGE \
	"    [file]         source code file\n" \
	"    [-o out file]  output file name, default is a.out\n" \
	"    [-S]           generate assembly code\n" \
	"    [-c]           generate not linked object file\n" \
	"    [-v]           show running commands\n" \
	"    [-C]           path to real compiler\n" \
	"    [-a]           path to assembler\n" \
	"    [-l]           path to linker\n" \
	"    [-h]           print this menu\n"

static void usage(bool small, FILE* stream)
{
	fprintf(stream, "%s", small ? SMALL_USAGE : SMALL_USAGE USAGE);
}

#define DEFAULT_OUT_NAME "a.out"
#define DEFAULT_BC1_PATH "./bc1"
#define DEFAULT_AS_PATH "as"
#define DEFAULT_LD_PATH "ld"

int main(int argc, char* argv[])
{
	const char* out_file = DEFAULT_OUT_NAME;
	const char* in_file  = NULL;

	const char* bc1_path = DEFAULT_BC1_PATH;
	const char* as_path  = DEFAULT_AS_PATH;
	const char* ld_path  = DEFAULT_LD_PATH;

	bool no_link = false;
	bool no_as   = false;
	bool verbose = false;

	if (argc < 2) {
		usage(true, stderr);
		exit(1);
	}

	in_file = argv[1];

	int c;
	while ((c = getopt(argc, argv, "o:ScvC:a:l:h")) != -1) switch (c) {
	case 'o':
		out_file = optarg;
		break;

	case 'C':
		bc1_path = optarg;
		break;

	case 'a':
		as_path = optarg;
		break;

	case 'l':
		ld_path = optarg;
		break;

	case 'S':
		no_as = true;

	case 'c':
		no_link = true;
		break;

	case 'v':
		verbose = true;
		break;

	case 'h':
		usage(false, stdout);
		exit(0);

	case '?':
		usage(true, stderr);
		exit(1);
	}

	FILE* sh = popen("sh", "w");

	char temp_s_buf[] = "/tmp/bc.XXXXXX.s";
	const char* temp_s = tmpnam(temp_s_buf);
	fprintf(sh, "%s %s %s\n", bc1_path, in_file, no_as ? out_file : temp_s);
	if (verbose)
		fprintf(stdout, "%s %s %s\n", bc1_path, in_file, no_as ? out_file : temp_s);

	if (no_as)
		goto done;

	char temp_o_buf[] = "/tmp/bc.XXXXXX.o";
	const char* temp_o = tmpnam(temp_o_buf);
	fprintf(sh, "%s %s -o %s\n", as_path, temp_s, no_link ? out_file : temp_o);
	if (verbose)
		fprintf(stdout, "%s %s -o %s\n", as_path, temp_s, no_link ? out_file : temp_o);

	if (no_link)
		goto done;

	fprintf(sh, "%s %s -o %s\n", ld_path, temp_o, out_file);
	if (verbose)
		fprintf(stdout, "%s %s -o %s\n", ld_path, temp_o, out_file);

done:
	pclose(sh);

	return 0;

temp_create_error:
	fprintf(stderr, "bc: can't create temp file.\n");
	return 2;
}
