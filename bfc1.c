/*
	Permission to use, copy, modify, and/or distribute this software for
	any purpose with or without fee is hereby granted.

	THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL
	WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
	FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
	DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
	AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
	OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MAX_PROG_SIZE 1024 * 32
#define MAX_VARS 1024 * 2
#define MAX_CMDS 1024 * 16
#define MAX_LABEL_NAME 32
#define MAX_LABELS 1024
#define MAX_CONST_STR 128
#define MAX_LEVEL_COUNT 128

enum var_type {
	VAR_PROG = 0,
	
	VAR_STACK,
	VAR_REGISTER,

	VAR_CONST,
	VAR_CONST_STR,

	VAR_LABEL,
};

enum cmd_size {
	CMD_BYTE = 0,
	CMD_WORD,
	CMD_DWORD,
	CMD_QWORD,
};

enum reg {
	REG_AH = 0,
	REG_AL,
	REG_CH,
	REG_CL,
	REG_DH,
	REG_DL,
	REG_BH,
	REG_BL,

	REG_RAX,
	REG_RDI,
	REG_RSI,
	REG_RDX,
	REG_RBP,
	REG_RSP,
};

struct var {
	enum var_type type;

	union {
		enum reg reg;
		size_t stack_offset;
		uint32_t value;
		char str[MAX_CONST_STR];
	} value;
};

struct var_lifetime {
	uint32_t start_instr;
	uint32_t end_instr;
};

enum cmd_type {
	CMD_ADD = 0,
	CMD_SUB,
	CMD_XOR,
	CMD_SYSCALL,
	CMD_LABEL,
	CMD_MOV,
	CMD_CMP,
	CMD_JE,
	CMD_JNE,
	CMD_JMP,
	CMD_LEA,
	CMD_PUSH,
	CMD_POP,
};

struct cmd {
	enum cmd_type type;
	enum cmd_size size;

	struct var* arg0;
	struct var* arg1;
	struct label* label;
};

enum label_type {
	LABEL = 0,
	LABEL_GLOBL,
};

struct label {
	enum label_type type;
	char name[MAX_LABEL_NAME];
};

static struct var bf_vars[MAX_VARS];
static struct var vars[MAX_VARS];
static size_t var_count = 0;

static struct cmd cmds[MAX_CMDS];
static size_t cmd_count = 0;

static struct label labels[MAX_LABELS];
static size_t label_count = 0;

static struct var* var_stdout;
static struct var* var_stdin;

static struct cmd* add_cmd(struct cmd* cmd)
{
	cmds[cmd_count++] = *cmd;
	return &cmds[cmd_count - 1];
}

static struct label* add_label(struct label* label)
{
	labels[label_count++] = *label;
	return &labels[label_count - 1];
}

static struct var* add_var(struct var* var)
{
	vars[var_count++] = *var;
	return &vars[var_count - 1];
}

static void compile_syscall(struct var* num, struct var* arg0, struct var* arg1, struct var* arg2)
{
#	define move_to_reg(x, y) \
		add_cmd(&(struct cmd) { .type = CMD_MOV, .arg0 = x, .size = CMD_QWORD,  \
				.arg1 = add_var(&(struct var) { .type = VAR_REGISTER, .value.reg = y })});

	move_to_reg(num, REG_RAX);
	if (arg0 != NULL) move_to_reg(arg0, REG_RDI);
	if (arg1 != NULL) move_to_reg(arg1, REG_RSI);
	if (arg2 != NULL) move_to_reg(arg2, REG_RDX);

#	undef move_to_reg

	add_cmd(&(struct cmd) { .type = CMD_SYSCALL });
}

static void clear_var(struct var* var)
{
	struct var* zero_var = add_var(&(struct var) { .type = VAR_CONST, .value.value = 0 });
	add_cmd(&(struct cmd) { .type = CMD_MOV, .size = CMD_BYTE, .arg0 = zero_var, .arg1 = var });
}

static void generate_prolouge(void)
{
	add_cmd(&(struct cmd) { .type = CMD_LABEL, .arg0 = NULL, .arg1 = NULL,
			.label = add_label(&(struct label) { .type = LABEL_GLOBL, .name = "_start" })});

	struct var* rbp = add_var(&(struct var) { .type = VAR_REGISTER, .value.reg = REG_RBP });
	struct var* rsp = add_var(&(struct var) { .type = VAR_REGISTER, .value.reg = REG_RSP }); 
	add_cmd(&(struct cmd) { .type = CMD_MOV, .size = CMD_QWORD, .arg0 = rsp, .arg1 = rbp } );

	var_stdin  = add_var(&(struct var) { .type = VAR_CONST, .value.value = 0 });
	var_stdout = add_var(&(struct var) { .type = VAR_CONST, .value.value = 1 });
}


static size_t level                          = 0;
static size_t level_reparts[MAX_LEVEL_COUNT] = {0};
static struct var* zero_var;

static void generate_loop_start(void)
{
	struct label* label = add_label(&(struct label) { .type = LABEL, .name = {0} });
	sprintf(label->name, "B%zu_%zu", level_reparts[level], level);
	add_cmd(&(struct cmd) { .type = CMD_LABEL, .arg0 = 0, .arg1 = 0, .label = label });

	struct var* ah = add_var(&(struct var) { .type = VAR_REGISTER, .value.reg = REG_AH });
	add_cmd(&(struct cmd) { .type = CMD_MOV, .size = CMD_BYTE, .arg0 = &bf_vars[var_count], .arg1 = ah });
	add_cmd(&(struct cmd) { .type = CMD_CMP, .size = CMD_BYTE, .arg0 = zero_var, .arg1 = ah });

	struct var* end_label = add_var(&(struct var) { .type = VAR_LABEL });
	sprintf(end_label->value.str, "E%zu_%zu", level_reparts[level], level);
	add_cmd(&(struct cmd) { .type = CMD_JE, .arg0 = end_label });

	level++;
}

static void generate_loop_end(void)
{
	level--;

	struct var* start_label = add_var(&(struct var) { .type = VAR_LABEL });
	sprintf(start_label->value.str, "B%zu_%zu", level_reparts[level], level);
	add_cmd(&(struct cmd) { .type = CMD_JMP, .arg0 = start_label });

	struct label* label = add_label(&(struct label) { .type = LABEL, .name = {0} });
	sprintf(label->name, "E%zu_%zu", level_reparts[level], level);
	add_cmd(&(struct cmd) { .type = CMD_LABEL, .arg0 = 0, .arg1 = 0, .label = label });

	level_reparts[level]++;
}

static void generate_print(void)
{
	struct var* rsi = add_var(&(struct var) { .type = VAR_REGISTER, .value.reg = REG_RSI });

	add_cmd(&(struct cmd) { .type = CMD_LEA, .size = CMD_QWORD, .arg0 = &bf_vars[var_count],
			.arg1 = rsi });

	compile_syscall(add_var(&(struct var) { .type = VAR_CONST, .value.value = 1 }), var_stdout,
			rsi, add_var(&(struct var) { .type = VAR_CONST, .value.value = 1 }));
}

static void generate_get(void)
{
	struct var* rsi = add_var(&(struct var) { .type = VAR_REGISTER, .value.reg = REG_RSI });

	add_cmd(&(struct cmd) { .type = CMD_LEA, .size = CMD_QWORD, .arg0 = &bf_vars[var_count], .arg1 = rsi });

	compile_syscall(add_var(&(struct var) { .type = VAR_CONST, .value.value = 0 }), var_stdin,
			rsi, add_var(&(struct var) { .type = VAR_CONST, .value.value = 1 }));
}

static void generate_epiloge(void)
{
	compile_syscall(add_var(&(struct var) { .type = VAR_CONST, .value.value = 60 }),
			zero_var, NULL, NULL);
}

static void compile(const char* str)
{
	generate_prolouge();

	zero_var = add_var(&(struct var) { .type = VAR_CONST, .value.value = 0 });

	clear_var(&bf_vars[0]);

	size_t var_count          = 0;
	size_t unique_var_counter = 0;

	for (const char* c = str; *c != '\0'; c++) switch (*c) {
	case '>':
		var_count++;
		if (var_count > unique_var_counter) {
			unique_var_counter++;
			clear_var(&bf_vars[var_count]);
		}

		break;

	case '<':
		var_count--;
		break;

	default:
		break;
	}

	var_count          = 0;
	unique_var_counter = 0;

	for (const char* c = str; *c != '\0'; c++) switch (*c) {
	case '>':
		var_count++;
		break;
	
	case '<':
		var_count--;
		break;

	case '+': {
		size_t add_count = 0;
		for (; *c == '+'; c++, add_count++);
		c--;

		add_cmd(&(struct cmd) {
				.type = CMD_ADD,
				.size = CMD_BYTE,
				.arg0 = add_var(&(struct var) {
						.type = VAR_CONST, .value.value = add_count}),
				.arg1 = &bf_vars[var_count],
		});

		break;
	}

	case '-': {
		size_t sub_count = 0;
		for (; *c == '-'; c++, sub_count++);
		c--;

		add_cmd(&(struct cmd) {
				.type = CMD_SUB,
				.size = CMD_BYTE,
				.arg0 = add_var(&(struct var) {
						.type = VAR_CONST, .value.value = sub_count}),
				.arg1 = &bf_vars[var_count],
		});

		break;
	}
	
	case '[':
		generate_loop_start();
		break;
	
	case ']':
		generate_loop_end();
		break;

	case '.':
		generate_print();
		break;

	case ',':
		generate_get();
		break;

	default:
		  break;
	}

	generate_epiloge();
}

static struct var_lifetime get_var_lifetime(struct var* var)
{
	struct var_lifetime lifetime;
	uint32_t last_usage;
	bool start_set = false;

	for (int i = 0; i < cmd_count; i++) {
		struct cmd* cmd = &cmds[i];
		struct var* found;

		if (cmd->arg0 != NULL) {
			found = cmd->arg0;
			goto process_founded;
		}
		if (cmd->arg1 != NULL) {
			found = cmd->arg1;
			goto process_founded;
		}

		continue;

process_founded:
		if (found != var)
			goto done;

		last_usage = i;
		
		if (!start_set) {
			lifetime.start_instr = i;
			start_set            = true;
		}

done:
		if (found == cmd->arg0 && cmd->arg1 != NULL) {
			found = cmd->arg1;
			goto process_founded;
		}
	}

	lifetime.end_instr = last_usage;

	return lifetime;
}

static size_t stack_offset = 0;

static bool try_dispence_with_registers(struct var* var, enum cmd_size size)
{
	return false;
}

static void dispence_with_stack(struct var* var)
{
	var->type = VAR_STACK;
	var->value.stack_offset = stack_offset += 8;
}

static void dispence_register(struct var* var, struct cmd* cmd)
{
	if (var->type != VAR_PROG)
		return;

	if (!try_dispence_with_registers(var, cmd->size))
		dispence_with_stack(var);
}

static void dispence_registers(void)
{
	for (int i = 0; i < cmd_count; i++) {
		struct cmd* cmd = &cmds[i];

		if (cmd->arg0 != NULL) dispence_register(cmd->arg0, cmd);
		if (cmd->arg1 != NULL) dispence_register(cmd->arg1, cmd);
	}
}

static size_t const_str_num = 0;

static void add_data_variable(struct var* var, FILE* file)
{
	if (var->type != VAR_CONST_STR)
		return;

	fprintf(file, "\tS%zu: .ascii \"%s\"\n", const_str_num++, var->value.str);
}

static void print_reg(enum reg reg, FILE* file)
{
	switch (reg) {
	case REG_AH:
		fprintf(file, "ah");
		break;

	case REG_AL:
		fprintf(file, "al");
		break;

	case REG_BH:
		fprintf(file, "bh");
		break;

	case REG_BL:
		fprintf(file, "bl");
		break;

	case REG_CH:
		fprintf(file, "ch");
		break;

	case REG_CL:
		fprintf(file, "cl");
		break;

	case REG_DH:
		fprintf(file, "dh");
		break;

	case REG_DL:
		fprintf(file, "dl");
		break;

	case REG_RAX:
		fprintf(file, "rax");
		break;

	case REG_RDI:
		fprintf(file, "rdi");
		break;

	case REG_RDX:
		fprintf(file, "rdx");
		break;

	case REG_RSI:
		fprintf(file, "rsi");
		break;

	case REG_RBP:
		fprintf(file, "rbp");
		break;

	case REG_RSP:
		fprintf(file, "rsp");
		break;

	default:
		break;
	}
}

static void print_const_str(struct var* var, FILE* file)
{
	size_t const_str = 0;

	for (int i = 0; i < cmd_count; i++) {
		struct cmd* cmd = &cmds[i];

		if (cmd->arg0 != NULL && cmd->arg0->type == VAR_CONST_STR) const_str++;
		if (cmd->arg0 == var)
			break;

		if (cmd->arg1 != NULL && cmd->arg1->type == VAR_CONST_STR) const_str++;
		if (cmd->arg1 == var)
			break;
	}

	fprintf(file, "S%zu", const_str - 1);
}

static void print_var(struct var* var, FILE* file)
{
	switch (var->type) {
	case VAR_CONST:
		fprintf(file, "$%i", var->value.value);
		break;
	
	case VAR_STACK:
		fprintf(file, "-%zu(%%rbp)", var->value.stack_offset + 1);
		break;

	case VAR_REGISTER:
		fprintf(file, "%%");
		print_reg(var->value.reg, file);
		break;

	case VAR_CONST_STR:
		print_const_str(var, file);
		break;

	case VAR_LABEL:
		fprintf(file, "%s", var->value.str);
		break;

	case VAR_PROG:
		fprintf(stderr, "bfc1: invalid internal state\n");
		exit(1);
	}
}

static void print_prefix(FILE* file, enum cmd_size* size)
{
	switch (*size) {
	case CMD_BYTE:
		fputc('b', file);
		break;

	case CMD_WORD:
		fputc('w', file);
		break;

	case CMD_DWORD:
		fputc('l', file);
		break;

	case CMD_QWORD:
		fputc('q', file);
		break;

	default:
		break;
	
	}
}

static void create_asm(FILE* file)
{
	fprintf(file, ".data\n");

	for (int i = 0; i < cmd_count; i++) {
		struct cmd* cmd = &cmds[i];

		if (cmd->arg0 != NULL) add_data_variable(cmd->arg0, file);
		if (cmd->arg1 != NULL) add_data_variable(cmd->arg1, file);
	}

	fprintf(file, ".text\n");
	
	for (int i = 0; i < cmd_count; i++) {
		struct cmd* cmd = &cmds[i];

		switch (cmd->type) {
		case CMD_ADD:	
			fprintf(file, "\tadd");
			print_prefix(file, &cmd->size);
			fputc(' ', file);
			print_var(cmd->arg0, file);
			fprintf(file, ", ");
			print_var(cmd->arg1, file);
			fprintf(file, "\n");

			break;

		case CMD_MOV:
			fprintf(file, "\tmov");
			print_prefix(file, &cmd->size);
			fputc(' ', file);
			print_var(cmd->arg0, file);
			fprintf(file, ", ");
			print_var(cmd->arg1, file);
			fprintf(file, "\n");

			break;

		case CMD_SUB:
			fprintf(file, "\tsub");
			print_prefix(file, &cmd->size);
			fputc(' ', file);
			print_var(cmd->arg0, file);
			fprintf(file, ", ");
			print_var(cmd->arg1, file);
			fprintf(file, "\n");

			break;

		case CMD_XOR:
			fprintf(file, "\txor");
			print_prefix(file, &cmd->size);
			fputc(' ', file);
			print_var(cmd->arg0, file);
			fprintf(file, ", ");
			print_var(cmd->arg1, file);
			fprintf(file, "\n");

			break;

		case CMD_SYSCALL:
			fprintf(file, "\tsyscall\n");
			break;

		case CMD_LABEL:
			if (cmd->label->type == LABEL_GLOBL)
				fprintf(file, ".globl %s\n", cmd->label->name);

			fprintf(file, "%s:\n", cmd->label->name);

			break;

		case CMD_JE:
			fprintf(file, "\tje ");
			print_var(cmd->arg0, file);
			fprintf(file, "\n");

			break;

		case CMD_JNE:
			fprintf(file, "\tjne ");
			print_var(cmd->arg0, file);
			fprintf(file, "\n");

			break;

		case CMD_JMP:
			fprintf(file, "\tjmp ");
			print_var(cmd->arg0, file);
			fprintf(file, "\n");

			break;

		case CMD_CMP:
			fprintf(file, "\tcmp");
			print_prefix(file, &cmd->size);
			fputc(' ', file);
			print_var(cmd->arg0, file);
			fprintf(file, ", ");
			print_var(cmd->arg1, file);
			fprintf(file, "\n");

			break;


		case CMD_PUSH:
			fprintf(file, "\tpush");
			print_prefix(file, &cmd->size);
			fputc(' ', file);
			print_var(cmd->arg0, file);
			fprintf(file, "\n");

			break;

		case CMD_POP:
			fprintf(file, "\tpop");
			print_prefix(file, &cmd->size);
			fputc(' ', file);
			print_var(cmd->arg0, file);
			fprintf(file, "\n");

			break;

		case CMD_LEA:
			fprintf(file, "\tlea");
			print_prefix(file, &cmd->size);
			fputc(' ', file);
			print_var(cmd->arg0, file);
			fprintf(file, ", ");
			print_var(cmd->arg1, file);
			fprintf(file, "\n");

			break;

		default:
			break;
		}
	}
}

int main(int argc, char* argv[])
{
	const char* in_path  = NULL;
	const char* out_path = NULL;

	if (argc != 3)
		return 1;

	in_path  = argv[1];
	out_path = argv[2];

	FILE* in = fopen(in_path, "r");
	if (in == NULL)
		return 1;

	fseek(in, 0L, SEEK_END);
	size_t size = ftell(in);
	fseek(in, 0L, SEEK_SET);

	char prog_buf[MAX_PROG_SIZE];
	fread(prog_buf, MAX_PROG_SIZE, size, in); 

	fclose(in);

	compile(prog_buf);
	dispence_registers();

	FILE* out = fopen(out_path, "w");
	if (out == NULL)
		return 1;

	create_asm(out);

	fclose(out);

	return 0;
}
