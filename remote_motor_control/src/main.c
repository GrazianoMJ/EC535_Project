#include "bluetooth.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef SELF_TEST
# include <stdlib.h> /* mkstemp */
# include <assert.h>
#endif

#define CONTROL_DEV_PATH "/dev/motor_control"
#define INVALID_COMMAND (0xff)
#define BUF_SIZE 32

/*
 * Map from bluetooth command values to control device file command IDs.
 */
static const unsigned char command_map[] =
{
	'F',
	'P',
	'U',
	'D',
	'L',
	'R'
};

struct Command
{
	unsigned char command_type;
	unsigned int magnitude; /* For commands that have a distance */
};

int control_fd = -1;

static struct Command
parse_message(const unsigned char* message, size_t message_size)
{
	struct Command cmd = { .command_type = INVALID_COMMAND, .magnitude = 0 };
	if (message_size == 2)
	{
		char arg1 = message[1];
		switch(message[0])
		{
		case 0:
		case 1:
			/* message[1] is don't-care for the first two command types */
			arg1 = 0;
			/* fall-through */
		case 2:
		case 3:
		case 4:
		case 5:
			cmd.command_type = command_map[message[0]];
			cmd.magnitude = arg1;
			break;
		}
	}

	return cmd;
}

static int
recv_msg(const unsigned char* message, size_t message_size)
{
	char buf[BUF_SIZE];
	struct Command cmd = parse_message(message, message_size);
	ssize_t write_count;

	if (cmd.command_type == INVALID_COMMAND)
	{
		return -1;
	}

	write_count = snprintf(buf, BUF_SIZE, "%c%u\n", cmd.command_type, cmd.magnitude);
	if (write_count >= BUF_SIZE)
	{
		fprintf(stderr, "Unable to format command for message %.*s\n",
				(int)message_size, message);
		return -1;
	}

	printf("Writing %d bytes: '%.*s'\n", write_count, write_count, buf);
	write_count = write(control_fd, buf, write_count);
	if (write_count == -1)
	{
		perror("write " CONTROL_DEV_PATH);
		return -1;
	}
	return 0;
}

#ifdef SELF_TEST
#define FAKE_DEV_FILE_BUF_SIZE 256
static void
run_tests(void)
{
	/* Message parsing tests */
	{
		struct Command cmd;

		cmd = parse_message((unsigned char[]){0, 0}, 2);
		assert(cmd.command_type == 'F');

		cmd = parse_message((unsigned char[]){1, 0}, 2);
		assert(cmd.command_type == 'P');

		cmd = parse_message((unsigned char[]){2, 7}, 2);
		assert(cmd.command_type == 'U');
		assert(cmd.magnitude == 7);

		cmd = parse_message((unsigned char[]){3, 6}, 2);
		assert(cmd.command_type == 'D');
		assert(cmd.magnitude == 6);

		cmd = parse_message((unsigned char[]){4, 5}, 2);
		assert(cmd.command_type == 'L');
		assert(cmd.magnitude == 5);

		cmd = parse_message((unsigned char[]){5, 4}, 2);
		assert(cmd.command_type == 'R');
		assert(cmd.magnitude == 4);

		/* Too many args */
		cmd = parse_message((unsigned char[]){2, 0, 0}, 3);
		assert(cmd.command_type == INVALID_COMMAND);

		/* Command type out of bounds */
		cmd = parse_message((unsigned char[]){6, 3}, 2);
		assert(cmd.command_type == INVALID_COMMAND);

		/* Command type out of bounds */
		cmd = parse_message((unsigned char[]){0xff, 2}, 2);
		assert(cmd.command_type == INVALID_COMMAND);

		/* Command too short */
		cmd = parse_message((unsigned char[]){0}, 0);
		assert(cmd.command_type == INVALID_COMMAND);

		/* Command missing an arg */
		cmd = parse_message((unsigned char[]){2}, 1);
		assert(cmd.command_type == INVALID_COMMAND);
	}

	/* Message handling tests */
	{
		unsigned char fake_dev_file[FAKE_DEV_FILE_BUF_SIZE] = "";
		ssize_t fake_dev_file_size;
		char fake_file_name[] = "/tmp/remotecontroltest.XXXXXX";
		control_fd = mkstemp(fake_file_name);
		assert(control_fd != -1);
		unlink(fake_file_name);

		// fire
		recv_msg((unsigned char[]){0, 0}, 2);
		lseek(control_fd, 0, SEEK_SET);
		fake_dev_file_size = read(control_fd, fake_dev_file, FAKE_DEV_FILE_BUF_SIZE);
		assert(fake_dev_file_size == 1);
		assert(fake_dev_file[0] == 'F');

		// Left
		ftruncate(control_fd, 0);
		lseek(control_fd, 0, SEEK_SET);
		recv_msg((unsigned char[]){4, 0x10}, 2);
		lseek(control_fd, 0, SEEK_SET);
		fake_dev_file_size = read(control_fd, fake_dev_file, FAKE_DEV_FILE_BUF_SIZE);
		assert(fake_dev_file_size == 3);
		assert(fake_dev_file[0] == 'L');
		assert(fake_dev_file[1] == '1');
		assert(fake_dev_file[2] == '6');

		close(control_fd);
		control_fd = -1;
	}
}
#endif

int
main(int argc, char **argv)
{
#ifndef SELF_TEST
	control_fd = open(CONTROL_DEV_PATH, O_WRONLY);
	if (control_fd == -1)
	{
		perror("open " CONTROL_DEV_PATH);
		return -1;
	}

	int ret = run_rfcomm_server(recv_msg);

	close(control_fd);
	return ret;
#else
	fprintf(stderr, "Running self tests...\n");
	run_tests();
	fprintf(stderr, "Self tests pass!\n");
	return 0;
#endif
}
